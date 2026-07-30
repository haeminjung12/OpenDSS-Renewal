import readline from "node:readline";

const endpoint = process.argv[2];

if (!endpoint) {
    console.error("Qt Creator MCP endpoint argument is required.");
    process.exit(2);
}

let sessionId;

function requestId(message) {
    return Array.isArray(message) ? null : (message?.id ?? null);
}

function writeError(message, error) {
    if (requestId(message) === null)
        return;

    process.stdout.write(`${JSON.stringify({
        jsonrpc: "2.0",
        id: requestId(message),
        error: {
            code: -32000,
            message: error.message,
        },
    })}\n`);
}

async function forward(line) {
    if (!line.trim())
        return;

    let message;
    try {
        message = JSON.parse(line);
    } catch (error) {
        console.error(`Invalid MCP JSON from Codex: ${error.message}`);
        return;
    }

    const headers = {
        Accept: "application/json, text/event-stream",
        "Content-Type": "application/json",
    };
    if (sessionId)
        headers["Mcp-Session-Id"] = sessionId;

    try {
        const response = await fetch(endpoint, {
            method: "POST",
            headers,
            body: line,
        });

        const newSessionId = response.headers.get("mcp-session-id");
        if (newSessionId)
            sessionId = newSessionId;

        const body = await response.text();
        if (!response.ok) {
            throw new Error(
                `Qt Creator MCP returned HTTP ${response.status}${body ? `: ${body}` : ""}`,
            );
        }

        if (body.trim())
            process.stdout.write(`${body.trim()}\n`);
    } catch (error) {
        writeError(message, error);
    }
}

const input = readline.createInterface({
    input: process.stdin,
    crlfDelay: Infinity,
});

let queue = Promise.resolve();
input.on("line", line => {
    queue = queue.then(() => forward(line));
});
