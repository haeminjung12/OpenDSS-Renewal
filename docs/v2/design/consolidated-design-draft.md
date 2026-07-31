# Open Droplet Sorting Suite (OpenDSS)
## OpenDSS v2 Consolidated Product Design Specification

**File:** `consolidated-design-draft.md`
**Document ID:** ODSS-DES-002  
**Version:** 1.0  
**Status:** User-Designated Master Specification  
**Date:** July 26, 2026  
**Primary platform:** Windows 11  
**Product:** Open Droplet Sorting Suite (OpenDSS)

> **Master authority:** On July 26, 2026, the user designated this file as the single master product and design specification for OpenDSS v2. Its requirements control implementation, design, review, and validation. Earlier source-status and authority-hierarchy commentary retained inside this document is provenance only and cannot override the consolidated requirements here.

> **Deviation rule:** If code, forms, plans, ledgers, derived documents, reviews, or proposed behavior deviate from this document, follow this document. If the applicable text is ambiguous, internally inconsistent, missing, or cannot be followed safely, stop the affected work and clarify with the user. Do not invent or retain a fallback.

---

## Approved amendment design integration

### Shell and shared presentation

- Start maximized. Restore Down is supported. A restored window enforces an exact minimum of 1600 × 900 logical px, may grow larger, and never resizes below that minimum. Visual, Qt Design Studio, and Computer Use validation may use maximized state and restored state at exactly 1600 × 900 or larger, never below.
- Keep the status header to one horizontal line. Each of Camera, DAQ, Active Model, and Current Activity uses icon, label, value, readiness color, and a non-color cue.
- Use a bottom-left Configuration overlay (the internal component type may remain `BottomHardwarePanel`) opened and closed by an arrow. Its visible panel heading and shell action are exactly `Configuration`; `Hardware` and `Hardware Configuration` are not user-visible names. It neither pushes the workspace nor spans the window.
- Present ordinary failures through `ErrorMessage` with primary text `Error`. Technical detail belongs only in the log under Settings > Diagnostics.

The startup Camera-unavailable prompt is a focused modal mock with the exact text `Camera unavailable. Continue?` and Yes/No actions. It appears at most once in a session.

### Capture and Label

Capture has one shared `DarkImageViewer` Camera preview and one visible right panel. `CollapsibleSection` headings for Single Image, Image Sequence, and Droplet Dataset Capture never disappear. Only their bodies collapse; multiple idle bodies may be open; the active body is forced open while other headings remain visible and disabled.

Label gives the center Droplet Crop grid visual dominance. The right panel uses `SelectedCropSection` and `ClassesFilterSection`. Selected Crop contains an enlarged crop, Class actions, Skip, Remove from Dataset, and Restore when applicable. Classes & Filter contains Class Names, counts, All, each applicable Class, Unlabeled, Skipped, and Removed. `CropThumbnail` keeps selection, focus, class, and crop state independent. Class 0 is blue, Class 1 orange, Class 2 purple; Unlabeled has no class border; Skipped and Removed have distinct gray treatments.

### Sequence Viewer

Sequence Viewer is a still-frame inspection surface. It shows current and total frame, Previous, Next, direct seek, zoom, pan, Fit, and 1:1. It has no Play, Pause, automatic progression, speed control, or frame-navigation lifecycle. Keyboard commands are Left/Right, Shift+Left/Right for 10 frames, Ctrl+Left/Right for 50, Home/End, plus/minus, F, and 1. Missing frames are skipped silently.

### Train, Library, and Active Model

Library owns Add Model and Import Model. Add Model uses one popup requiring a nonblank unique Name, one supported Architecture, and one of exactly two factual Starting Weights choices: `ImageNet` or `Pretrained`. Each choice resolves to its own fixed, bundled, architecture-specific local checkpoint. The Starting Weights choice itself is not a Library model; separately, a successful fresh installation registers two complete ready-to-run three-class pretrained Model Packages as specified in §2.1.1. A choice is enabled only when its corresponding bundled artifact is locally valid. No Starting Weights action downloads, substitutes, or falls back to the network. Train shows every Library model as selectable, consumes the selected model read-only for identity, architecture, and factual Starting Weights, and trains that selected model after Dataset, Compute Device, and Output Location are selected. Train does not compatibility-filter Library models or show compatibility warnings/reasons. While Training it shows progress, timing, effective device, one `TrainingPlot` for Training/Validation Loss, and one for Validation Accuracy. Successful completion atomically creates the newly named package and makes it Active; failure leaves source identity/artifacts intact and shows `Error` and Retry Save without an Active success state.

`ModelListRow` shows exactly Model Name, Architecture, and Class Type. Class Type is rendered exactly as `2 Class` or `3 Class`; extra descriptions, class-name lists, and classes prose are absent from the row. A selected row has a darker background and selection never activates it. Active state remains factual in the global header and selected-model detail rather than adding a fourth row field. `SelectedModelSection` is collapsible and contains name, Active state, architecture, Starting Weights, trained date, source Dataset, classes, Training results, package location, and actions.

Model Test and Sequence Test display the Active Model as read-only context and contain no local Model selector.

### Live and Decision Boundary

Live keeps one workspace. Its right panel contains Setup Profile, Run Information, Trigger & Timing, Output & Recording, and Running disclosures. Running is collapsed before Start. Start collapses setup, expands Running, keeps the explicitly authorized Trigger & Timing controls editable for subsequent droplets, keeps Set Decision Boundary available for one-click replacement, and retains the Camera preview. Unlisted setup fields become read-only. `CameraActionBar` below the preview contains Start Camera and Start Sorting.

Trigger & Timing contains Active Model, Hit Class, independent Trigger Every Droplet and DAQ Output toggles, Send Test Pulse, and `Decision Boundary`. `Set Decision Boundary` arms exactly one placement click in the owning Camera frame; ordinary frame clicks do nothing. The clicked source-image X/Y point begins a horizontal observer/comparison segment that extends to the right edge only. `Top is Hit` or `Bottom is Hit` selects the Observed Route mapping, and `Reset` clears the boundary. Decision Boundary affects only Observed Route; it never changes Predicted Class, Decision, or DAQ output.

Running contains status, elapsed time, Total Droplets, Rejected, Predicted Class counts, Decision Hit/Waste, Observed Hit/Waste/Unresolved, Camera FPS, Inference Time, Pause/Resume, and Stop.

### Sequence Test, Results, Settings, and Configuration

Sequence Test reuses Live disclosure styling without Camera controls. Its dedicated section contains Load Sequence, name, first-frame preview, frame count, recorded FPS, Processing FPS, available memory, buffer size, Load to Memory, load status, its own editable Decision Boundary, Start, and Stop. During Running, the viewer remains visibly populated and follows current processing with the newest renderable processed frame; intermediate preview frames may be dropped only to prevent preview lag, while scientific processing, event decisions, and trigger timing remain unchanged. It does not reuse or modify Live Decision Boundary state. The custom picker shows valid OpenDSS sequence folders, thumbnails, name, count, recorded FPS, and status. Physical DAQ Output is an unchecked checkbox by default, and Sequence Test inference defaults to CPU.

Results keeps the loaded Run in the center. In `RunListSection` on the right, only the selectable Run list scrolls; the bottom Load and Remove Run actions remain fixed and continuously available. After explicit confirmation, Remove Run moves the complete selected Run folder to the Windows Recycle Bin; it never performs direct permanent deletion. Selection and loaded content are visually distinct. Center detail uses factual groups and tables rather than first-class charts or an event browser.

Settings uses a centered column with Storage, Application Information, and Diagnostics only.

`BottomHardwarePanel` Camera content is Status, Device, Resolution preset, conditional Custom Width/Height, Bit Depth default 8-bit for a new/default state, Exposure, Readout mode default Fastest, and a section titled exactly `LUT`. Saved profiles retain their supported Bit Depth. LUT behavior remains preview-only and no numeric LUT values are displayed beneath its title. DAQ content is Status, auto-detected Device, conditional Device selector, Output Channel, and reported maximum voltage range/frequency. Only adapter-supported settings appear.

### Shared component plan

```text
StatusHeaderItem
CollapsibleSection
BottomHardwarePanel
DarkImageViewer
CameraActionBar
ErrorMessage
CropThumbnail
SelectedCropSection
ClassesFilterSection
ModelListRow
SelectedModelSection
HitBoundaryOverlay
RunListSection
TrainingPlot
```

Create each component only in the first authorized slice with an immediate consumer. All remain token-driven and Qt Design Studio-editable; runtime behavior stays outside `.ui.qml`.

## Document conventions

The normative terms in this specification are used as follows:

- **MUST** and **MUST NOT** identify requirements grounded in the controlling OpenDSS v2 product, information-architecture, interaction, workflow, or accessibility baselines.
- **SHOULD** and **SHOULD NOT** identify consolidated design recommendations. A deviation requires a documented design or technical rationale and must not alter approved product behavior.
- **MAY** identifies an optional presentation or implementation choice that does not change navigation, workflow, scientific meaning, operation behavior, resource ownership, or file contracts.

Source references use these short labels after their first full citation:

- **PM** — *OpenDSS Approved v2 Product Model*, Version 1.0.
- **IA** — *OpenDSS v2 Information Architecture and Screen Inventory*.
- **LF** — *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification*.
- **WF** — *OpenDSS Detailed User Workflow Specification*, Version 1.0.
- **PDS v0.1** — *OpenDSS Product Design Specification*, Draft v0.1. This is supporting design evidence only.
- **Repository** — `https://github.com/haeminjung12/OpenDSS_clean`. This is optional implementation and historical design evidence only.

The approved product sequence is always represented as:

```text
Predicted Class
    → Decision
        → Observed Route
```

The required Decision values are **Hit** and **Waste**. The required Observed Route values are **Hit**, **Waste**, and **Unresolved**.

---

## Table of contents

1. [Document status, purpose, scope, and authority](#1-document-status-purpose-scope-and-authority)
2. [Users and experience principles](#2-users-and-experience-principles)
3. [Approved information architecture](#3-approved-information-architecture)
4. [Application shell](#4-application-shell)
5. [Visual design system](#5-visual-design-system)
6. [Shared component system](#6-shared-component-system)
7. [Visual state language and fault communication](#7-visual-state-language-and-fault-communication)
8. [Workspace design framework](#8-workspace-design-framework)
9. [Data > Capture design](#9-data--capture-design)
10. [Data > Label design](#10-data--label-design)
11. [Data > Sequence Viewer design](#11-data--sequence-viewer-design)
12. [Models > Train design](#12-models--train-design)
13. [Models > Model Test design](#13-models--model-test-design)
14. [Models > Library design](#14-models--library-design)
15. [Sort > Live design](#15-sort--live-design)
16. [Sort > Sequence Test design](#16-sort--sequence-test-design)
17. [Results > Runs design](#17-results--runs-design)
18. [Settings design](#18-settings-design)
19. [Bottom Configuration panel](#19-bottom-configuration-panel)
20. [Setup Profile design](#20-setup-profile-design)
21. [Contextual workflow links](#21-contextual-workflow-links)
22. [Keyboard, focus, and repeated-work behavior](#22-keyboard-focus-and-repeated-work-behavior)
23. [Responsive, high-DPI, and accessibility requirements](#23-responsive-high-dpi-and-accessibility-requirements)
24. [Qt Design Studio and design-handoff contract](#24-qt-design-studio-and-design-handoff-contract)
25. [Required mock-data and design-review states](#25-required-mock-data-and-design-review-states)
26. [Design QA and acceptance evidence](#26-design-qa-and-acceptance-evidence)
27. [Requirements and source traceability](#27-requirements-and-source-traceability)
28. [Design-input disposition appendix](#28-design-input-disposition-appendix)
29. [Explicit exclusions](#29-explicit-exclusions)
30. [Source register and citation index](#source-register-and-citation-index)

---

# 1. Document status, purpose, scope, and authority

## 1.1 Status and purpose

This document consolidates the approved OpenDSS v2 product structure, information architecture, workspace inventory, low-fidelity interactions, application-state behavior, and compatible visual-design evidence into one coherent product design specification.

It is written for:

- product and scientific stakeholders reviewing the OpenDSS v2 user experience;
- interaction, visual, and accessibility designers;
- Qt Design Studio authors and Qt application developers;
- application-state, hardware-integration, and workflow engineers who need an authoritative presentation contract;
- validation and quality-assurance personnel preparing design and interaction acceptance evidence;
- documentation and publication teams that need stable user-facing terminology and screen behavior.

This document does not reopen approved product decisions D-001 through D-019. It translates those decisions into a design system, component system, workspace design, responsive behavior, accessibility contract, mock-data contract, and review evidence.

## 1.2 Consolidation provenance and internal interpretation

The following historical authority order was used to consolidate the source material into this specification:

1. *OpenDSS Approved v2 Product Model* controls all approved product decisions D-001 through D-019.
2. *OpenDSS v2 Information Architecture and Screen Inventory* and *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* control the current shell, navigation, workspace inventory, interaction behavior, application states, disabled-action behavior, contextual handoffs, resource ownership, and locking model unless they directly conflict with PM.
3. *OpenDSS Detailed User Workflow Specification* supplies scientific, workflow, artifact, persistence, recovery, nonfunctional, and acceptance requirements that do not conflict with the three sources above.
4. *OpenDSS Product Design Specification*, Draft v0.1, is a design-review input only. It may inform compatible visual identity, color, typography, spacing, components, accessibility, responsive behavior, Qt Design Studio authoring, mock-data, and visual-regression guidance. It does not define product behavior.
5. The existing repository is optional implementation evidence and a potential source of reusable implementation assets. It does not control navigation, workspace names, editable settings, terminology, product scope, or application state.

These source relationships explain provenance; they do not allow a downstream agent to override a requirement in this master specification by reopening an incorporated source. If two requirements in this document appear to conflict, or a requirement is ambiguous, missing, or unsafe to follow, stop the affected work and clarify with the user.

## 1.3 What this specification controls after approval

After approval, this specification will control:

- application-shell composition and visual hierarchy;
- visual tokens, typography, spacing, geometry, iconography, and surface treatment;
- shared component anatomy, variants, states, and accessibility behavior;
- workspace composition and visual hierarchy for every approved workspace and distinct Capture mode;
- presentation of Empty, Unavailable, Ready, operation-lifecycle, completion, interruption, and failure states;
- presentation of selection, keyboard focus, class identity, labeling state, removal state, and disabled state;
- direct disabled reasons, inline validation, contextual fault banners, progress, busy actions, and preservation messaging;
- responsive behavior at supported desktop sizes and Windows scaling factors;
- keyboard, focus, pane-resizing, and repeated-work interaction behavior;
- design-time mock data, component-gallery coverage, screen-state previews, and design-review evidence;
- Qt Design Studio design-handoff boundaries needed to keep visual assets editable without embedding business logic in form files.

## 1.4 What remains outside scope

This specification does not define or alter:

- scientific algorithms, detector logic, Droplet Crop generation logic, routing logic, trajectory logic, model architectures, or training configurations;
- canonical JSON or CSV schemas except where their existing contracts determine visible content;
- production service architecture, threading, queue implementations, hardware adapters, memory management, persistence internals, packaging or installer technology beyond the explicit installation contract in §2.1.1, security architecture, or CI/CD;
- qualified hardware models, driver versions, or laboratory safety procedures;
- a final public wordmark or trademark decision;
- new product features, alternate navigation, additional workflows, or future-release placeholders.

The old PDS v0.1 was a design-review input. It is explicitly not the basis of product behavior. Repository modules and existing screens are likewise not product-design authority.

**Source basis:** *OpenDSS Approved v2 Product Model* §1 and §§19–21; *OpenDSS v2 Information Architecture and Screen Inventory* document authority and §§7–8; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* document authority and §§21–23; *OpenDSS Detailed User Workflow Specification* §§1–4 and §§73–78; *OpenDSS Product Design Specification*, Draft v0.1, scope and §§11–12, as supporting design evidence only.

---

# 2. Users and experience principles

## 2.1 User model

After successful installation, OpenDSS v2 is a local, offline, single-user scientific application. The primary user is one laboratory scientist operating one local application instance on Windows 11. The design MUST NOT imply accounts, authentication, permissions, collaborative roles, cloud workspaces, project ownership, review queues, or project-management hierarchy.

The following descriptions are task contexts, not permission-bearing roles:

| Task context | Typical goals | Design implications |
|---|---|---|
| Laboratory operator | Connect available hardware, capture data, configure a Run, sort droplets, monitor operations, stop or recover | Hardware and operation state must be factual, prominent, and internally consistent. Primary actions must expose one direct blocker when unavailable. |
| Dataset reviewer | Define two or three classes, label or relabel Droplet Crops, Skip, Remove from Dataset, restore, and inspect Image Counts | Repeated work must be keyboard-efficient; large collections must be virtualized; selection, label, Skip, and Removed states must remain visually distinct. |
| Model developer | Define MobileNetV3-Small or EfficientNet-B0 identities in Library, train them from approved Starting Weights, inspect factual metrics, run Model Test, and manage local Model Packages | Library-owned identity and Train's read-only consumption must be clear without exposing tuning controls or scientific approval states. |
| Troubleshooter | Inspect runtime, driver, file, and diagnostic facts and recover from technical faults | Plain-language operational messages must provide direct recovery while technical detail remains available through Diagnostics and ordinary files. |

### 2.1.1 Installation and network contract

OpenDSS application installation does not require Python or a completed Training Environment and MUST NOT fail or block because Python, the Training Environment, internet access for training dependencies, or any training package is absent or broken. The installer adds no product-authored unsigned-installer, SHA-256-verification, SmartScreen-instruction, or Windows/organizational-security-policy notice. The installer is a small bootstrap installer. It bundles the exact repository-owned OpenDSS `droplet_trainer` wheel identified by the authoritative training-environment lock and two complete ready-to-run three-class pretrained Model Packages: one MobileNetV3-Small package and one EfficientNet-B0 package. It does not embed an offline Python runtime, third-party wheelhouse, or complete CPU/GPU training payload.

The installer presents exactly this ordered flow:

1. **Welcome**;
2. **Prerequisite Check**;
3. **OpenDSS installation**;
4. **Final Verification**.

Prerequisite Check covers only requirements for installing the OpenDSS application. It uses aligned status rows with consistent label, status, explanation, and action columns. Its exact introductory copy is **OpenDSS requires the following software runtimes. Next is available only when all three are Ready.**

It contains exactly three required software-runtime rows:

1. **DCAM API Runtime**;
2. **NI-DAQmx Runtime**; and
3. **Microsoft Visual C++ x64 Runtime**.

Each row shows exactly **Ready** or **Missing** plus a factual explanation. **Ready** means the required runtime software passed the applicable architecture-aware software-presence, supported-version, and loadability checks defined below. **Missing** means it was absent or failed an applicable architecture/version/loadability check. When a row is Missing, its explanation is respectively **Install or repair the DCAM API Runtime, then select Check Again.**, **Install or repair the NI-DAQmx Runtime, then select Check Again.**, or **Install or repair the Microsoft Visual C++ x64 Runtime, then select Check Again.** The aligned actions are **Open DCAM Page**, **Open NI-DAQmx Page**, **Install VC++ Runtime**, and **Check Again**.

**Next** is disabled whenever any one of the three rows is **Missing** and becomes enabled only while all three are **Ready**. While blocked, the exact prominent summary is **Installation is blocked until all required software runtimes are Ready.** When all three pass, it is **All required software runtimes are Ready.** The installer may install the bundled Microsoft Visual C++ x64 Runtime from its row action, then rerun the same detection; DCAM API and NI-DAQmx remain user-installed external software.

On x64 Windows, **DCAM API Runtime** detection resolves the native x64 Windows `System32\DCAMAPI.DLL` path without WOW64 filesystem redirection, checks that exact file's presence and x64 PE architecture, and may additionally enforce an approved supported file-version rule when one exists. A 32-bit installer MUST NOT determine readiness by loading the native x64 DLL, MUST NOT use its redirected `{sys}` view as the native path, and MUST NOT accept a SysWOW64-only or wrong-architecture DLL. It does not copy a DLL, edit vendor registration, invoke an unofficial installer, or require a 32-bit DCAM runtime. One authoritative detection function and result supplies both Prerequisite Check and Final Verification. Its durable diagnostic log records installer process architecture, Windows architecture, exact resolved native path, existence, detected file version, architecture/version findings, and final Ready/Missing decision.

The installer MUST NOT open DCAM or NI-DAQmx to enumerate hardware and MUST NOT probe, require, infer, or report a connected Camera, DCAM device/driver device-presence, connected DAQ hardware, NVIDIA/GPU/CUDA, Python, Training Environment, training packages, training compute, or internet connectivity for Training. NVIDIA/CUDA and all Training Environment diagnosis belong exclusively to the post-install **Repair Training Environment** flow. The installer never silently installs DCAM API, NI-DAQmx, NVIDIA, or attached-device drivers. Python is never presented as an installer prerequisite.

The installer creates one visible Start Menu action named exactly **Set Up or Repair OpenDSS Training**. It may use a thin launcher plus a separate application-owned script when required for safe execution. The installer Final Verification page MAY offer one optional action whose exact visible label is **Repair Training Environment**; it launches the same post-install setup/repair flow, and application installation success is independent of its execution or outcome.

Every launch of **Set Up or Repair OpenDSS Training** diagnoses first, in stage order, and reports what is **Ready**, **Missing**, or **Broken** before modifying anything:

1. exact pinned CPython `3.12.10`;
2. application-owned environment at `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`;
3. locked bundled OpenDSS `droplet_trainer` wheel;
4. the other 36 authoritative pinned, hash-locked third-party wheels in the persistent resumable application-owned wheel cache;
5. exact 37-wheel environment installation; and
6. authoritative environment check.

After diagnosis, the tool repairs or installs only missing or invalid stages, preserves every valid existing stage, and continues automatically through every remaining stage to Final Verification. When CPython is missing or invalid, it downloads exact pinned CPython `3.12.10`, verifies its authoritative SHA-256, and installs it before creating or repairing the environment. It verifies every cached wheel against its authoritative hash before reuse or installation.

The tool is idempotent and resumable after network or process failure. It shows the current stage and per-package progress, including package name and ordinal/total; uses bounded retry behavior for transient failures; supports Cancel, Retry, and Repair; stops cancellation safely; retains already verified cached wheels; removes or disregards incomplete temporary downloads; preserves all valid components and all user data/Active Model; and writes a persistent diagnostic log. Repair reuses every hash-verified cached artifact and downloads only missing or invalid artifacts; it MUST NOT redownload the complete wheel set.

Every failure identifies the exact stage. A package download failure identifies the package, source URL, and underlying error; verification, Python installation, environment creation, package installation, and final environment-check failures are distinguished. Dependency download failure, interruption, or cancellation MUST NOT be reported as **Python installation failed**. The tool never silently claims readiness.

A missing, partial, or failed Training Environment never rolls back or invalidates the installed OpenDSS application, local runtime, datasets, or ready-to-run Model Packages. Training and Model Test remain truthfully unavailable until the shared authoritative diagnosis passes. Their disabled navigation explanation directs the user to **Settings > Training Environment**, where **Repair Training Environment** launches the same application-owned setup/repair tool. After successful setup, Training is local and has no runtime dependency download or network fallback. Existing trained-model local/no-network runtime requirements are unchanged.

On a successful fresh installation, both bundled Model Packages are installed under the standard Models data root and registered in Library. Both are immediately loadable by Model Test, Live, and Sequence Test without Training. Each declares exactly three immutable Class IDs; Class Names are user-facing display labels only and never determine package compatibility, inference, sorting, Hit Class identity, or DAQ behavior. The MobileNetV3-Small package is the one Active Model; the EfficientNet-B0 package is registered, valid, ready to use, and inactive. Two-class ready packages are deferred and are not created by this contract.

Reinstall and upgrade preserve all existing user datasets, Model Packages, and Model Registry data under the Documents data root without overwriting, replacing, deleting, or resetting them. They do not change an existing Active Model. Uninstall leaves those Documents user-data folders and registry data intact; removing the application MUST NOT remove user datasets or models.

Final Verification preserves its complete structure and aligned result rows with these exact labels: **OpenDSS**, **DCAM API Runtime**, **NI-DAQmx Runtime**, **Microsoft Visual C++ x64 Runtime**, **MobileNetV3-Small 3-Class Model**, **EfficientNet-B0 3-Class Model**, **Library Registration**, and **Active Model**. It displays **Ready** or the exact failure for each row. The three runtime rows reuse the same authoritative detection results used by Prerequisite Check; DCAM in particular MUST NOT be recomputed through a different path or a DLL load. Final Verification never probes or reports connected hardware, DCAM driver/device presence, Camera/DAQ operation, NVIDIA/GPU/CUDA, Python, Training Environment health, training packages, or effective Training compute, and it never reports an unavailable runtime, package, registration, or Active Model as ready. **Repair Training Environment** may appear as the optional handoff to the separate post-install verifier; it is not an installer verification result. That action and final navigation actions appear in one consistent bottom action area; no Repair action floats beside or between verification rows. Final Verification MUST NOT be reduced to only OpenDSS and bundled-model lines.

Every installer-visible product/version label is exactly `0.9.1`.

## 2.2 Experience principles

### EP-01 — State before decoration

The interface MUST make the current artifact, hardware availability, operation ownership, lifecycle state, and next valid action apparent before brand decoration or secondary detail.

### EP-02 — Scientific authority remains with the scientist

OpenDSS MUST present factual measurements and technical compatibility. It MUST NOT visually imply that a Dataset, Model Package, Run, class distribution, or result is approved, rejected, certified, suitable, promoted, or scientifically acceptable.

### EP-03 — One workspace, one dominant job

Each workspace MUST have one dominant purpose and one state-dependent primary action. A workspace may expose secondary file or contextual actions, but it must not become a generic dashboard.

### EP-04 — Technical blocking only

An operation may be unavailable only for a direct technical reason, such as missing hardware, an unreadable or incompatible artifact, resource ownership, an unavailable runtime, or an unwritable output location. Scientific measurements must never become blocking validation.

### EP-05 — Context near control

A reason, validation message, status, or recovery action SHOULD appear near the action or content it affects. Global status is summarized once in the header. Operation faults appear once in the affected workspace.

### EP-06 — Prediction, action, and observation remain separate

Predicted Class, Decision, and Observed Route MUST have distinct labels, grouping, and metric treatments. Class identity must not be used as a substitute for routing Decision or physical observation.

### EP-07 — Failure includes preservation and recovery

Interrupted and Failed presentations MUST state what stopped, the direct reason when known, whether partial data was preserved, and the next direct action. A failed write must never be styled as successful.

### EP-08 — Ordinary files remain visible and credible

Paths, folder actions, canonical artifact names, and file-state messages SHOULD reinforce that Datasets, Image Sequences, Model Packages, Setup Profiles, Model Test outputs, and Runs are ordinary user-owned Windows files and folders.

### EP-09 — Keyboard efficiency for repeated work

High-frequency labeling and viewer interactions MUST be keyboard operable. All core workflows must remain keyboard accessible. Global shortcuts must not fire while the user is typing in a field.

### EP-10 — Restrained brand, strong operational hierarchy

Brand colors SHOULD guide identity and primary attention. Semantic state, selection, keyboard focus, class identity, and review state must use separate visual channels. Light application surfaces and dark image-view canvases should provide a stable scientific-workbench character.

### EP-11 — One authoritative state projection

The header, panel, workspace, enabled actions, counters, locks, and banners MUST agree. The design must not imply that widgets carry independent copies of authoritative domain state.

### EP-12 — Preserve work without creating workflow bureaucracy

Draft selections may persist during the current session, and artifacts may be preselected through contextual links. These conveniences MUST NOT become projects, mandatory wizards, approval workflows, or startup workspace restoration.

**Source basis:** *OpenDSS Approved v2 Product Model* §§2–3, §§13–14, and §17; *OpenDSS v2 Information Architecture and Screen Inventory* §§1–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§1–4 and §§17–20; *OpenDSS Detailed User Workflow Specification* §§2, 4–6, 9, 36, and 38–43; *OpenDSS Product Design Specification*, Draft v0.1 §2 as adapted design evidence.

---

# 3. Approved information architecture

## 3.1 Primary and secondary navigation

The application MUST preserve this structure exactly:

```text
Data
├── Capture
├── Label
└── Sequence Viewer

Models
├── Train
├── Model Test
└── Library

Sort
├── Live
└── Sequence Test

Results
└── Runs

Settings
```

There is no Home screen and no separate Reports or Sort Setup workspace.

The Capture workspace contains one shared live Camera preview and three collapsible operation sections in the right-side panel:

```text
▸ Single Image
▸ Image Sequence
▸ Droplet Dataset Capture
```

These are section headings, not selectors and not a third navigation tier. Every heading remains visible. Sections expand independently, multiple sections may be open while idle, and none is expanded by default.

Live pre-run, Starting, Running, Paused, Stopping, Completed, Interrupted, and Failed presentations are states of one `Sort > Live` workspace. They are not separate screens or navigation destinations.

## 3.2 Navigation purpose

| Navigation item | Concise purpose |
|---|---|
| **Data** | Acquire source images, label Datasets, and visually inspect recorded Image Sequences. |
| **Data > Capture** | Use one shared live-camera workspace to save a Single Image, record an Image Sequence, or capture a Dataset. |
| **Data > Label** | Define two or three classes and assign Labels to Droplet Crops in one Dataset. |
| **Data > Sequence Viewer** | Step, seek, zoom, pan, Fit, and inspect a recorded Image Sequence at 1:1 without hardware output. |
| **Models** | Create, test, and manage local two-class or three-class Model Packages. |
| **Models > Train** | Train one existing Library-defined MobileNetV3-Small or EfficientNet-B0 model identity from eligible Labeled Droplet Crops without mutating source packages. |
| **Models > Model Test** | Measure classification behavior of one model on one compatible labeled Dataset without changing Active Model. |
| **Models > Library** | Inspect and manage valid v2 Model Packages and the one global Active Model. |
| **Sort** | Run live physical sorting or reprocess a recorded Image Sequence through sorting logic. |
| **Sort > Live** | Configure, start, monitor, pause, stop, and review the immediate result of one Live Sorting Run. |
| **Sort > Sequence Test** | Process an Image Sequence through fixed processing and routing logic, optionally issuing physical DAQ output, and create a Run. |
| **Results** | Review persisted sorting-operation outputs only. |
| **Results > Runs** | List and inspect Live Sorting and Sequence Test Runs. |
| **Settings** | Manage Storage and inspect Application Information and Diagnostics. |

## 3.3 Navigation behavior

- Primary navigation MUST remain visible in every normal workspace.
- Selecting a primary domain exposes its approved secondary items and opens the selected persistent workspace.
- Direct navigation MUST remain available; contextual workflow links may preselect an artifact but must not create a mandatory wizard.
- Navigation away from an active operation MUST NOT stop it.
- Returning to the owning workspace MUST restore its current operation presentation and controls.
- A conflicting Start action in another workspace MUST be disabled with **Another operation is active**.
- The selected workspace must be shown through text, icon state, and a structural selection indicator that is distinct from keyboard focus.
- The rail MAY offer compact icon-only and expanded icon-plus-label presentations, provided labels remain available through tooltips and accessible names and the approved hierarchy does not change.

## 3.4 Startup

Every application launch MUST open:

```text
Data > Capture
```

Normal launch from the installed shortcut or directly from the production OpenDSS executable displays only the GUI and MUST NOT create, retain, or flash a Command Prompt/console window. Explicit user-invoked diagnostic or command-line modes may use a console when that console is part of the requested mode. This launch contract does not hide installer progress or errors, suppress persistent diagnostic logs, or remove supported diagnostic entry points. Normal application exit leaves no orphan console or helper process.

The previously open workspace MUST NOT be restored. Persisted domain state, including the Active Model and valid storage preferences, may remain available because it is not workspace restoration.

**Source basis:** *OpenDSS Approved v2 Product Model* §4, §5, and D-001, D-006, D-007, D-011, D-013, D-014, D-017, and D-018; *OpenDSS v2 Information Architecture and Screen Inventory* §§1–3; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§2.2–2.4 and §18; *OpenDSS Detailed User Workflow Specification* §§7–9 as amended by *OpenDSS Approved v2 Product Model*; *OpenDSS Product Design Specification*, Draft v0.1 §3 is not carried forward except for compatible compact/expanded rail presentation.

---

# 4. Application shell

## 4.1 Shell composition

The application shell MUST contain:

1. a compact global status header;
2. persistent primary navigation;
3. the current workspace region;
4. a workspace-owned operation-side panel where applicable;
5. a shell-owned slide-out Camera/DAQ panel;
6. one contextual fault-banner location inside the affected workspace.

```text
┌──────────────────────────────────────────────────────────────────────────────────┐
│ Camera: <status> | DAQ: <status> | Active Model: <name> | Activity: <value>      │
│                                                              [ Configuration ]   │
├──────────────────┬──────────────────────────────────────┬────────────────────────┤
│ PRIMARY NAV      │ WORKSPACE REGION                     │ OPERATION-SIDE PANEL   │
│                  │                                      │                        │
│ Data             │ Live preview / crop collection /     │ Inputs, selected-item │
│ Models           │ sequence viewer / metrics /          │ details, status,      │
│ Sort             │ model list / Run content / settings  │ counters, actions     │
│ Results          │                                      │                        │
│ Settings         │                                      │                        │
├──────────────────┴──────────────────────────────────────┴────────────────────────┤
│ Contextual fault banner occupies the affected workspace, not a notification hub.│
└──────────────────────────────────────────────────────────────────────────────────┘
                                                          ┌────────────────────────┐
│ CONFIGURATION                │
                                                          │ Camera                 │
                                                          │ DAQ                    │
                                                          └────────────────────────┘
```

## 4.2 Region design

| Region | Required design treatment | Interaction behavior |
|---|---|---|
| **Global status header** | Compact, always visible, visually quieter than the active workspace but readable at a glance. Four status items use labels plus values, not color-only chips. | Projects authoritative Camera, DAQ, Active Model, and Current Activity state. The Configuration action opens the panel when permitted. |
| **Primary navigation** | Stable vertical rail with clear domain grouping. Selected state is structural and text-supported. | Opens approved workspaces directly. It remains usable during an operation when navigation does not violate resource locks. |
| **Workspace region** | Receives the largest flexible area and contains the dominant scientific content. | Retains the owning workspace state within the session. Does not use fixed desktop-coordinate positioning. |
| **Operation-side panel** | Stable right-side region for setup, selected-item detail, progress, counters, and actions. Its heading names the current task or state. | Content changes by mode or lifecycle state. Camera and DAQ technical controls never appear here. |
| **Configuration panel** | Overlays from the bottom-left above the workspace; keeps header context visible. | Provides the only user-editable Camera, DAQ, and approved Detector Configuration settings. Camera and DAQ remain independently lockable by device. During Live they are locked, while approved Detector Configuration remains available. |
| **Fault banner** | Full-width within the affected workspace's content boundary, above the affected content or immediately below its local header. | Persists until the user starts a new operation, opens a replacement artifact, or performs a provided recovery action. |

## 4.3 Recommended geometry

The following values are consolidated design recommendations, not changes to product behavior:

| Element | Recommended balanced value | Allowed behavior |
|---|---:|---|
| Global header height | 52–56 logical px | May increase only when system scaling or text wrapping requires it. |
| Navigation rail, compact | 56–64 logical px | Icons with tooltips and accessible labels. |
| Navigation rail, expanded | 200–224 logical px | Icon plus full label; user-selectable or responsive. |
| Operation-side panel | 360–440 logical px | Resizable; may collapse where the workspace remains operable. |
| Configuration panel | 420–520 logical px | Overlay; it must remain contained within the maximized window without hiding the global header. |
| Major splitter hit area | At least 8 logical px, with a 1 px visible divider | Keyboard resizing and reset/collapse alternatives required. |
| Workspace content padding | 16–24 logical px | 12 px may be used in dense data regions, not around destructive controls. |

The workspace region MUST remain flexible. Geometry must not rely on one fixed desktop resolution.

## 4.4 Global status header

The header MUST show exactly these status areas:

```text
Camera | DAQ | Active Model | Current Activity
```

Representative values are:

| Area | Values |
|---|---|
| Camera | `Unavailable`, `Connected`, `Streaming` |
| DAQ | `Unavailable`, `Ready`, `Active` |
| Active Model | `No Active Model` or `<Model Name>` |
| Current Activity | `Idle`, `Capturing Image`, `Recording Sequence`, `Droplet Dataset Capture`, `Labeling`, `Training`, `Testing Model`, `Testing Sequence`, `Sorting`, `Paused` |

`Starting` and `Stopping` do not add header vocabulary. The approved operation activity remains visible until resources release.

Each status item SHOULD include:

- an accessible name combining label and current value;
- a restrained state symbol or icon;
- text that remains visible in the maximized layout;
- a tooltip with fuller factual detail when the compact value is insufficient.

The header MUST NOT become an editable settings surface or a notification center.

## 4.5 Primary-action placement

The current primary action MUST appear in a consistent final-action region at the bottom of the operation-side panel or equivalent selected-item panel. The action must remain visible without covering content. When disabled, one short reason appears immediately beneath or adjacent to it.

Only one action is visually primary in a local state. Pause, Resume, Stop, Automatic Save, Set Active, or a completion action may become primary when the state changes.

## 4.6 Startup workspace treatment

On launch, the selected navigation state and preview region MUST describe `Data > Capture`, with Single Image, Image Sequence, and Droplet Dataset Capture headings visible and all three sections collapsed. When the Camera is unavailable, the preview remains in place and displays **Camera unavailable**; the product does not replace the workspace with a Home or hardware-setup screen.

## 4.7 Focus order

The default shell-level focus order SHOULD be:

1. global status header and Configuration action;
2. selected primary and secondary navigation;
3. workspace-local heading and utility actions;
4. primary workspace content;
5. operation-side panel fields and actions;
6. contextual banner actions when present.

`F6` SHOULD cycle among the major shell regions without changing the selected workspace. When the Configuration panel closes, focus MUST return to the action that opened it.

**Source basis:** *OpenDSS Approved v2 Product Model* §§5 and 14; *OpenDSS v2 Information Architecture and Screen Inventory* §1; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §2, §§4–5, and §16; *OpenDSS Detailed User Workflow Specification* §§8–9 and §34; *OpenDSS Product Design Specification*, Draft v0.1 §§4–5 as adapted visual and geometry evidence.

---

# 5. Visual design system

## 5.1 Visual direction

OpenDSS v2 SHOULD use a restrained scientific-workbench visual language:

- a light application shell and light content surfaces;
- dark image and camera-view canvases;
- deep navy for identity and structural emphasis;
- royal blue for primary action, links, and keyboard focus;
- teal as a limited flow or brand accent;
- neutral grays for structure and metadata;
- semantic colors reserved for technical state and outcome;
- class identity represented by dedicated class tokens plus text or symbols, never by selection or focus color.

The droplet-and-branch identity is carried forward as a compatible brand concept. It must remain subordinate to operational content and must not be mechanically reduced into unreadable detail at small icon sizes.

## 5.2 Consolidated color tokens

The values below are recommended review tokens. They SHOULD be centralized and may be refined during contrast validation without changing their semantic roles.

### Brand and foundation

| Token | Review value | Intended use | Constraint |
|---|---:|---|---|
| `brand.navy.900` | `#0B1F52` | Logo, high-emphasis headings, structural identity | Not a default full-workspace background. |
| `brand.blue.600` | `#2563EB` | Primary action, link, keyboard focus, active interactive affordance | Must not double as class identity. |
| `brand.teal.600` | `#0D9488` | Limited brand/flow accent | Must not be the only Ready cue. |
| `brand.teal.500` | `#14B8A6` | Illustrative accent and non-text support | Do not use with white text unless contrast passes. |
| `brand.sky.300` | `#7DD3FC` | Subtle tint, noncritical visualization support | Not body text on white. |

### Light application surfaces

| Token | Review value | Intended use |
|---|---:|---|
| `surface.canvas` | `#F4F7FB` | Application background |
| `surface.primary` | `#FFFFFF` | Panels, tables, operation panels, dialogs |
| `surface.subtle` | `#F8FAFC` | Secondary rows, grouped fields, empty states |
| `surface.selected` | `#EFF6FF` | Selection fill, always paired with selection structure |
| `text.primary` | `#0F172A` | Main body and data |
| `text.secondary` | `#475569` | Labels, explanatory text |
| `text.muted` | `#64748B` | Lower-priority metadata that still passes contrast |
| `border.subtle` | `#E2E8F0` | Dividers and panel boundaries |
| `border.strong` | `#CBD5E1` | Splitters, focused grouping, stronger separation |

### Dark image/view surfaces

| Token | Review value | Intended use |
|---|---:|---|
| `viewer.canvas` | `#000000` | Shared full-size image-viewer canvas for Capture, Live, Sequence Test, Sequence Viewer, and Label selected crop |
| `viewer.surface` | `#111827` | Viewer controls and overlays |
| `viewer.text` | `#F8FAFC` | Viewer labels and factual overlays |
| `viewer.textMuted` | `#CBD5E1` | Secondary viewer metadata |
| `viewer.divider` | `#334155` | Viewer control separation |

### Semantic state

| Token | Review value | Meaning | Required non-color cue |
|---|---:|---|---|
| `state.neutral` | `#64748B` | Not configured, inactive, or not required | Text label or neutral symbol |
| `state.ready` | `#0F766E` | Connected and technically ready | Ready text and check/connection symbol |
| `state.active` | `#1D4ED8` | Accepted operation in progress | Activity label, progress/motion, and Stop availability |
| `state.paused` | `#B45309` | Operation paused | Paused label and pause symbol |
| `state.completed` | `#15803D` | Expected output finalized | Completed label and check symbol |
| `state.interrupted` | `#B45309` | Operation ended early; recoverable output may exist | Interrupted label and preservation statement |
| `state.failed` | `#B91C1C` | Technical failure | Failed label, error symbol, and direct reason |
| `state.unavailable` | `#64748B` | Technical prerequisite unsatisfied | Unavailable text and disabled action reason |

Color assignments MUST be validated against actual backgrounds. Semantic colors are not approval or scientific-quality states.

## 5.3 Class identity tokens

OpenDSS supports two-class and three-class workflows. The design system MUST provide `class.0`, `class.1`, and `class.2` tokens with:

- a distinct hue or pattern;
- a persistent `0`, `1`, or `2` text marker;
- the current Class Name where space permits;
- a compact symbol suitable for thumbnails and matrix headers;
- light and dark-surface variants that meet non-text contrast requirements.

Class identity MUST NOT use the same border or fill as:

- selected state;
- keyboard focus;
- Ready, Active, Completed, Interrupted, or Failed state;
- Skipped or Removed state.

The exact class hues MAY be refined with scientific users. Their semantic role and non-color labeling are required.

## 5.4 Typography

A neutral, highly legible sans-serif SHOULD be used throughout the application. Inter is the recommended design baseline when bundled and rendered consistently; an approved Windows/Qt fallback with comparable metrics may be used.

| Role | Recommended style | Typical size / line height |
|---|---|---|
| Application/product title | Semibold | 22–24 / 30–32 logical px |
| Workspace title | Regular | 22–24 / 30–32 |
| Major section title | Semibold | 16–18 / 22–26 |
| Section title | Semibold | 14–16 / 20–22 |
| Body and controls | Regular or medium | 16 / 20 |
| Ordinary setting label | Regular | 15 / 18 |
| Button text | Semibold | 16 / 18–20 |
| Caption, status, warning, and metadata | Regular | 13 / 16–18 |
| Metric value | Semibold, tabular figures | 22–32 / context |
| Table data | Regular, tabular figures where numeric | 13–14 / 18–20 |
| Path, filename, log, schema, channel | Monospace fallback | 13–14 / 18–20 |

Text Size offers exactly **Small (80%)**, **Medium (100%, default)**, and **Large (125%)**. **200% remains validation-only and is not selectable.** SettingsRepository and the UI integration must expose only 80, 100, and 125. Before publication or persistence, legacy persisted values must normalize as 90 → 100 and 150/175/200 → 125. The UI must never silently expose an unsupported value.

One application-wide text scale drives every typography role; workspace-local or component-local scale systems are prohibited. There is no blanket `10 pt` minimum for non-title text. DESIGN must establish one coherent, restrained hierarchy from actual visual evidence at maximized and restored window sizes of at least `1600 × 900`, preserving distinct title, body/control, caption/status/metadata, table, path, and log roles. The accepted hierarchy must correct clipping and crowding while remaining legible and consistent; it must not enlarge every role to a common floor or flatten the title hierarchy.

Numeric counters, timestamps, frame indices, Class Scores, Inference Time, FPS, and matrices SHOULD use tabular figures. Monospace must be limited to values that benefit from fixed-width alignment; it is not a general status typeface.

Text MUST not be converted into raster assets. Labels must remain translatable and scalable even if localization is not part of the first release.

## 5.5 Spacing and density

The recommended spacing scale is:

```text
4, 8, 12, 16, 20, 24, 32, 40 logical px
```

- Default control height SHOULD be 36 logical px.
- Dense data rows MAY use 32 logical px when labels and target sizes remain usable.
- Prominent actions and touch-adjacent targets SHOULD use 40 logical px or greater.
- Related label/control pairs SHOULD use 4–8 px internal spacing.
- Field groups SHOULD use 16–24 px separation.
- Major workspace regions SHOULD use 20–32 px separation or a splitter.

Density MUST not reduce target sizes below the accessibility minimum or cause text clipping at supported scaling factors.

## 5.6 Geometry, dividers, and elevation

| Element | Recommended geometry |
|---|---|
| Buttons and fields | 6 px radius |
| Panels and metric tiles | 8 px radius |
| Panels and dialogs | 10–12 px radius |
| Standard border | 1 logical px |
| Focus indicator | 2 logical px minimum with sufficient offset/contrast |
| Viewer overlay | 6–8 px radius with dark translucent surface |

Spacing and surface contrast SHOULD define hierarchy before borders. Dividers SHOULD be used for lists, tables, and split regions, not to outline every group. Elevation and shadow are reserved for floating panels, menus, tooltips, and dialogs; ordinary panels should remain flat or minimally elevated.

## 5.7 Iconography

Icons SHOULD be simple geometric line or restrained filled symbols with consistent stroke, corner, and optical weight.

- Standard visual sizes: 16, 20, and 24 logical px.
- Icon-only controls MUST use a 32–40 px target and have a tooltip and accessible name.
- Operational icons must be paired with text for Start, Pause, Resume, Stop, destructive actions, and fault recovery.
- A dedicated small OpenDSS mark SHOULD be used for 16–24 px application contexts. The full droplet-and-branch artwork must not be mechanically reduced until detail becomes illegible.
- Icons must not be used as the only distinction among Hit, Waste, and Unresolved, or among Class IDs.

## 5.8 Image and camera-view treatment

Camera previews, Droplet Crop inspection, and full-frame sequence viewing SHOULD use the dark viewer canvas. The image must remain visually dominant. Viewer controls and factual overlays should use dark translucent surfaces that do not modify the source image.

A camera-unavailable viewer retains the same geometry and uses centered factual text:

> **Camera unavailable**

Missing or unreadable frames retain the viewer and identify the frame/path problem without substituting decorative imagery.

## 5.9 Motion

Motion MUST be functional and restrained:

- panel opening and panel collapse MAY animate over a short duration;
- progress indicators MAY animate while work is indeterminate;
- operation-state changes SHOULD not use celebratory or attention-grabbing motion;
- reduced-motion preference MUST remove nonessential movement and replace animated transitions with immediate state changes or short fades;
- camera and sequence content is not considered decorative motion, but playback remains explicitly controllable.

**Source basis:** *OpenDSS Approved v2 Product Model* §§3, 5, 10, and 14; *OpenDSS v2 Information Architecture and Screen Inventory* §§1 and 4; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§1–5; *OpenDSS Detailed User Workflow Specification* §§4, 6, 8, 34, and 38–43; *OpenDSS Product Design Specification*, Draft v0.1 §§1, 5, 7, 9, and 10 as adapted design evidence.

---

# 6. Shared component system

## 6.1 Component governance

Approved workspaces MUST be assembled from shared components. One-off variants of buttons, fields, status badges, panels, thumbnails, tables, and fault treatments are defects unless a documented design rationale is approved.

Every interactive component MUST define:

- default, hover, pressed, selected where applicable, keyboard focus, disabled, and busy states;
- accessible name, role, value, and state;
- logical Tab behavior and keyboard activation;
- minimum target size and 200% scaling behavior;
- direct disabled-reason behavior where the component starts an operation;
- light and dark-surface use where applicable.

## 6.2 Shell and navigation components

| Component | Anatomy and variants | Required behavior |
|---|---|---|
| **Navigation item** | Icon, label, optional disclosure, selected indicator, compact/expanded variants | Selected and keyboard-focus treatments remain distinct. Tooltip and accessible name are required in compact mode. Disabled navigation is avoided; inaccessible operations are explained inside the workspace. |
| **Global status item** | Label, value, optional state symbol, optional detail tooltip | Read-only projection of authoritative state. Color is supplemental. Long model names use end or middle elision with full text available. |
| **Configuration action** | Configuration icon plus label at standard widths; icon plus tooltip at minimum width | Opens the shell-owned Configuration panel. It remains available during Live for Detector Configuration even while Camera and DAQ sections are locked. |
| **Configuration-panel section** | Section heading, status, supported field groups, applied/validation feedback, lock-owner explanation, optional collapse | Camera and DAQ sections remain independently editable or locked according to ownership. Detector Configuration exposes only the approved `Minimum Size` control. Unsupported properties are absent rather than shown as placeholders. |
| **Operation-side panel** | Heading, optional artifact summary, field groups, status/metrics, final action region | Content changes by workspace/mode/state. Primary action remains in a consistent lower region. It never duplicates Camera or DAQ settings. |

## 6.3 Input and action components

| Component | Variants and states | Required design behavior |
|---|---|---|
| **Button** | Primary, secondary, ghost, destructive; default, hover, pressed, focus, disabled, busy | One primary per local state. Disabled operation button exposes one adjacent reason. Busy state preserves label context, prevents double activation, and exposes status to assistive technology. |
| **Icon button** | Neutral, selected, destructive | Minimum 32 px preferred target; tooltip and accessible name required. Icon-only destructive action is not permitted without a text confirmation context. |
| **Capture disclosure section** | Fixed heading; collapsed, expanded, focus, disabled, active-owner states; independently scrolling body | Used for Single Image, Image Sequence, and Droplet Dataset Capture. All three headings remain visible; multiple idle sections may be expanded; none is expanded by default; an active section is forced open while the other headings are disabled. |
| **Segmented control** | Equal options, selected, focus, disabled | Used only for comparable peer choices that require single selection. All peer labels remain visible at supported widths. Arrow keys move among options; activation follows platform-consistent single-selection behavior. |
| **Text field** | Empty, populated, hover, focus, invalid, disabled, read-only | Persistent label; placeholder is not the label. Commit behavior is explicit. Error text appears below the field and remains until corrected or reverted. |
| **Number field** | Text entry plus optional step controls, unit label | Enforces supported range/format without silently changing a valid prior value. Units remain visible. Arrow/step behavior must not apply while disabled. |
| **Path field** | Read-only or editable path, Browse, Reveal/Open action | Middle-elides long paths; full path available by tooltip, selection, or accessible description. File/folder picker is standard Windows behavior. |
| **Combo box** | Closed, open, focused, invalid, disabled | Selected value retains discriminating text. Long device/model names remain inspectable. |
| **Checkbox** | Unchecked, checked, mixed where genuinely applicable, focus, disabled | Used for independent options such as Record Full Image Sequence and Physical DAQ Output. The label is clickable and included in the accessible name. |
| **Toggle** | Off/on, focus, disabled | Reserved for immediate binary settings where changing the value applies immediately. It is not used for operation Start/Stop or scientific choices. |

## 6.4 Content components

| Component | Anatomy and variants | Required design behavior |
|---|---|---|
| **Panel** | Header, optional summary, content, optional action footer | Uses spacing and surface hierarchy, not decorative borders. |
| **Inspector** | Selected-item title, large preview or metadata, action groups | Resizable and collapsible where the main task remains usable. Selection change updates content without stealing focus unexpectedly. |
| **List row** | Primary label, metadata, optional status/active marker, selection | Selected, Active Model, locked, and error conditions use separate cues. Double activation may invoke the row's primary open action when documented. |
| **Data table** | Header, sortable columns only where specified, rows, selection, empty/error states | Headers remain visible; numeric content aligns consistently; keyboard row navigation and accessible column names required. No first-class charts are substituted for required tables. |
| **Dataset thumbnail** | Image, Class ID/Name badge, label-state symbol, selection layer, focus ring, optional missing-image state | Selection, focus, class identity, Labeled, Skipped, and Removed cues are orthogonal. Images are virtualized. Removed retains an X/removed badge and subdued image; Skipped uses its own label/symbol. |
| **Sequence timeline** | Track, current-position thumb, buffered/available range where applicable, current/total labels | Keyboard step and seek alternatives required. Visual frame navigation is separate from Sequence Test processing. |
| **Metric tile** | Label, large value, optional unit, optional secondary factual context | Uses tabular figures. Does not apply pass/fail styling to scientific metrics. |
| **Progress display** | Determinate bar, current/total, stage label, elapsed/estimated time where available | Must distinguish progress from success. Indeterminate state names the current initialization/finalization stage. |
| **Log or diagnostic stream** | Timestamp/source/message rows, copy/open-folder actions where supported | Used only in Diagnostics or technical detail surfaces, not as a required normal-workflow console. Monospace is appropriate. |
| **Status badge** | Text plus icon/symbol, compact and standard variants | Semantic state only. Must not be used for scientific approval, model quality, or class identity. |
| **Empty state** | Factual title, short explanation, one direct action where applicable | Does not use marketing illustration in operational workspaces. Distinguishes no artifact from fault. |

## 6.5 Feedback and overlay components

| Component | Anatomy and variants | Required design behavior |
|---|---|---|
| **Direct disabled reason** | One short line adjacent to the disabled primary action; optional tooltip duplicates it | Updates when state changes. Shows the first applicable blocker, not a checklist. It remains perceivable by screen readers. |
| **Inline validation** | Field outline/icon, concise reason, optional correction hint | Invalid hardware edits revert to the last successfully applied value. Required selections are not silently guessed. Scientific measurements never become field errors. |
| **Contextual fault banner** | Outcome heading, direct reason, preservation statement, one or two recovery/inspection actions | One persistent banner per affected workspace. No repeated modal cascade and no notification center. |
| **Dialog** | Title, scoped explanation, content, primary/safe action, secondary/cancel | Used only when necessary: destructive model deletion, file overwrite supplied by normal file behavior, or comparable explicit confirmation. Safe action receives initial focus. |
| **Slide-out panel** | Title, close action, Camera and DAQ sections, status, supported controls | Focus is contained while open. Escape closes when permitted. Closing restores focus. Live forces closure and disables reopening. |
| **Splitter** | Visible divider plus larger invisible hit area, collapse/reset affordance where useful | Pointer drag, keyboard increment, and explicit collapse/reset alternatives required. |
| **Tooltip** | Short text, no interactive content | Required for ambiguous icons, compact navigation, elided values, and disabled-reason duplication. It cannot be the only way to obtain a critical blocker. |

### 6.5.1 Full-size image viewer navigation

All shared full-size image viewers use the same presentation-only navigation contract:

- `Ctrl`+wheel zooms around the pointer position.
- Scale `1.0` is the viewer's current fit-to-window presentation.
- Zoom is clamped to `0.3` through `10`.
- When zoomed content exceeds the viewport, normal wheel input scrolls vertically and `Shift`+wheel scrolls horizontally.
- Each axis uses scrollbar policy `AlwaysOn` exactly while that axis's content exceeds its viewport extent and `AlwaysOff` otherwise. Both scrollbars are interactive. During overflow they remain persistently visible and clickable; transient or `AsNeeded` presentation is prohibited. They support pointer thumb dragging and pointer click scrolling.
- Existing pan remains available alongside scrolling.
- Dataset grids, thumbnail grids, and other thumbnail presentations are excluded.
- Zoom and navigation never change source-image coordinates, persisted artifacts, detector values, or operational geometry.
- Exactly one shared production `FullSizeImageViewer` implementation serves the five full-size consumers: Capture, Live, Sequence Test, Sequence Viewer, and Label selected crop. Per-workspace viewer or hot-path JavaScript duplication is prohibited.
- The canvas/background inside that shared full-size viewer is black (`#000000`) for exactly those five consumers. This replaces only the former blue/navy canvas. Surrounding panels, grids and thumbnails, placeholder text, overlays, controls, and borders remain unchanged.

## 6.6 Component state requirements

- Disabled components retain readable labels and sufficient contrast; they must not disappear when their absence would obscure a prerequisite.
- Busy actions preserve width and context. A spinner may accompany the current verb, for example `Starting…` or `Saving…`.
- Destructive actions use text plus icon, destructive semantic color, and explicit scope.
- Read-only factual values use normal text contrast and a distinct non-editable treatment, not the visual style of disabled/unavailable content.
- Hover is supplemental. Every hover-only affordance must have a keyboard and non-hover path.
- Context menus MAY be used for secondary package or list actions only when the same actions remain discoverable through the selected-item panel or overflow menu.

## 6.7 Component accessibility

All components MUST:

- expose an accessible name that uses approved terminology;
- expose current value, checked state, selected state, expanded state, and disabled state where applicable;
- show visible keyboard focus with at least 3:1 contrast against adjacent colors;
- remain usable at 200% Windows scaling without clipping essential text or targets;
- avoid color-only distinction;
- support a minimum 24 × 24 logical-pixel target, with 32–40 px preferred for normal controls;
- preserve predictable focus after actions, selection changes, panel closure, and dialog dismissal.

**Source basis:** *OpenDSS Approved v2 Product Model* §§5, 7, 9, 13, and 14; *OpenDSS v2 Information Architecture and Screen Inventory* §§1, 3–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§2–5 and detailed workspace sections 6–15; *OpenDSS Detailed User Workflow Specification* §§8–9, 11–23, 34, and 38–43; *OpenDSS Product Design Specification*, Draft v0.1 §6 and §§9–12 as adapted component and accessibility evidence.

---

# 7. Visual state language and fault communication

## 7.1 Operational and presentation states

| State | Meaning | Visual treatment | Required text or structural cue |
|---|---|---|---|
| **Neutral** | Inactive, not configured, not required, or factual background context | Neutral text/surface; no semantic emphasis | Plain label such as `Idle`, `Not selected`, or `Not required` |
| **Unavailable** | A technical prerequisite is unsatisfied | Content remains visible; primary action disabled; muted semantic treatment | One direct reason such as **Camera unavailable** |
| **Ready** | Sufficient technical prerequisites exist for the primary operation | Normal editable controls; restrained ready cue; primary action enabled | `Ready` when useful and enabled action |
| **Active** | Operation accepted and Running | Active status badge, progress/counters, locked inputs, Stop available | `Running` or approved activity name |
| **Paused** | Same operation retained; defined processing/output is stopped | Amber/paused semantic treatment, stable counters, Resume dominant | `Paused`, pause icon, and explanatory text |
| **Completed** | Expected canonical output finalized | Completed treatment and direct next actions | `Completed` or factual stopped/completed status |
| **Interrupted** | `Error`; technical details are logged. | Persistent warning-level banner | `Interrupted`, reason, preservation result |
| **Failed** | `Error`; technical details are logged. | Persistent error-level banner | `Failed`, direct reason, preservation result |

`Empty` is a workspace condition indicating that no artifact has been selected. It is not an error and should use an Empty State component rather than Failed styling.

`Starting` and `Stopping` are operation lifecycle states. They use the Active language plus a stage label and busy/progress treatment. They must not appear as Ready or Completed.

## 7.2 Interaction and content states

| State | Purpose | Distinct visual channel |
|---|---|---|
| **Selection** | Identifies the item or row currently acted upon | Selected fill and/or structural side indicator plus selected state exposure |
| **Keyboard focus** | Identifies the control that receives keyboard input | High-contrast focus ring with offset; never replaced by selection fill |
| **Class identity** | Identifies Class ID 0, 1, or 2 and its Class Name | Dedicated class token plus text/number badge |
| **Labeled** | Crop has a committed Label | Class badge plus labeled/check symbol; not selection ring |
| **Skipped** | Crop deferred from training eligibility | `Skipped` text or `S` badge plus distinct neutral/pause-like symbol |
| **Removed** | Crop excluded while file and entry remain | Muted image overlay, `Removed` text or X symbol; restore action available |
| **Disabled** | Action/control cannot be used in current state | Reduced emphasis but readable; direct reason for primary operations |

Selection, focus, class identity, review state, and removal state MUST NOT rely on the same border color or fill.

## 7.3 Disabled-reason presentation

A disabled primary action MUST display one short reason immediately adjacent to the action. Examples:

```text
[ Start Sorting — disabled ]
Camera unavailable
```

```text
[ Start Model Test — disabled ]
The selected model has 2 output classes, but the selected Dataset defines 3 classes.
```

General blocker priority is:

1. the same workspace is Starting, Running, Paused, or Stopping;
2. another operation or resource lock conflicts;
3. required hardware is unavailable or not Ready;
4. a required artifact is absent, unreadable, or incompatible;
5. a required workflow selection is missing;
6. the output location is not writable.

Workspace-specific reason order in LF §16 takes precedence. Field-level validation may appear simultaneously, but the action must not show a readiness checklist.

## 7.4 Contextual fault banner

The banner appears at the top of the affected workspace content, below the workspace title or persistent Capture section headings and above the main regions. It contains:

1. the operation or action that stopped;
2. the direct technical reason when known;
3. whether partial data was preserved;
4. one or two direct recovery or inspection actions.

Example:

```text
┌──────────────────────────────────────────────────────────────────────┐
│ Droplet Dataset Capture interrupted                                          │
│ The camera disconnected. Existing frames and crops were preserved.  │
│                                                                      │
│ [ Open Dataset ]  [ Open Folder ]                                    │
└──────────────────────────────────────────────────────────────────────┘
```

The heading, reason, and actions must be keyboard reachable and exposed as an assertive but nonrepeating status update to assistive technology. The banner persists until superseded by a new operation/artifact or resolved by an action.

## 7.5 Recovery-action placement

- Open, reveal, or inspect actions appear inside the banner when they directly relate to preserved output.
- A retry/start-new action may appear in the banner or the operation panel, but not both as competing primary actions.
- Hardware reconnection is not represented as a fake in-product repair action; the UI states the prerequisite and updates when authoritative hardware state changes.
- Diagnostics links may appear for training/runtime faults but remain secondary to the direct operational action.

## 7.6 Busy actions and progress

- Start actions must become busy immediately after acceptance to prevent double activation.
- Inputs and conflicting operation controls lock at Start acceptance, not after the operation reaches Running.
- Starting progress names concrete stages such as `Creating Run folder`, `Loading model`, or `Initializing writer` when available.
- Stopping progress communicates that new processing/output has stopped and files are being flushed or finalized.
- Determinate progress uses current and total values where known. Indeterminate progress must still name the stage.
- Scientific metrics are not progress indicators unless they genuinely represent completion.

## 7.7 Partial-data preservation messaging

Completed, Interrupted, and Failed presentations must state preservation factually:

- `Run Summary and Droplet Log were finalized.`
- `Existing frames and Droplet Crops were preserved.`
- `A partial Run folder was preserved; the Droplet Log could not be finalized.`
- `No output was created.`

The interface must not infer preservation from the presence of a folder alone. It must reflect the persistence/recovery result reported by the authoritative operation state.

**Source basis:** *OpenDSS Approved v2 Product Model* §§3.2–3.3, 10, 13, and 14; *OpenDSS v2 Information Architecture and Screen Inventory* §§4 and 6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§3–4, §16, and §19; *OpenDSS Detailed User Workflow Specification* §§4, 8–9, 31–34, and 40–41; *OpenDSS Product Design Specification*, Draft v0.1 §§6–7 as adapted visual-state evidence.

---

# 8. Workspace design framework

## 8.1 Common workspace contract

Every workspace or distinct mode in Sections 9–18 is specified using this contract:

| Contract field | Required content |
|---|---|
| **Design purpose** | Why the workspace exists and what it must make visually clear. |
| **Main user goal** | The one dominant job. |
| **Major regions** | Primary scientific content, supporting content, and operation-side panel. |
| **Dominant visual hierarchy** | Which region and information receive the most visual weight. |
| **Operation-side-panel content** | Inputs, artifact summary, progress, counters, and state-dependent actions. |
| **Primary action by state** | The one dominant action in Empty, Ready, Running, Paused, Completed, or other applicable states. |
| **Secondary actions** | Valid file, contextual, inspection, or management actions. |
| **Required artifact or hardware** | Technical prerequisites only. |
| **Output artifact** | Canonical artifact or factual absence of one. |
| **Direct disabled reasons** | One operation-level blocker at a time. |
| **Fault-banner placement** | Where the one minimal `Error` presentation appears. |
| **Applicable states** | Only states that genuinely apply. |
| **Keyboard and focus behavior** | Logical order, repeated-work keys, and focus restoration. |
| **Resizing and collapse** | Splitter, panel, inspector, and minimum-width behavior. |
| **Mock-data states** | Minimum representative states required for design review. |
| **Next likely contextual action** | Optional direct handoff that preselects an artifact without creating a wizard. |

## 8.2 Common composition pattern

Operational workspaces SHOULD use this composition where applicable:

```text
WORKSPACE TITLE / LOCAL UTILITY ACTIONS
OPTIONAL MODE OR VIEW SELECTOR
OPTIONAL CONTEXTUAL FAULT BANNER

┌──────────────────────────────────────────────┬────────────────────────────┐
│ PRIMARY SCIENTIFIC CONTENT                   │ OPERATION / DETAIL PANEL   │
│                                              │                            │
│ Viewer, collection, metrics, table,          │ Inputs, selected artifact, │
│ master list, or Run content                  │ status, counters, actions  │
│                                              │                            │
└──────────────────────────────────────────────┴────────────────────────────┘
```

A workspace MAY use master-detail, three-region, or full-width layouts when required by its content, but the approved operation-side-panel role and shell remain stable.

## 8.3 State applicability matrix

| Workspace or mode | Empty | Unavailable | Ready | Starting | Running | Paused | Stopping | Completed | Interrupted | Failed |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| Single Image | — | ✓ | ✓ | transient busy | — | — | — | ✓ | — | ✓ |
| Image Sequence | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Droplet Dataset Capture | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Label | ✓ | ✓ | ✓ | — | — | — | — | — | — | ✓ |
| Sequence Viewer | ✓ | — | ✓ | — | ✓ | ✓ | — | ✓ | — | ✓ |
| Train | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| Model Test | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| Library | ✓ | ✓ where locked/unreadable | ✓ | — | — | — | — | — | — | ✓ |
| Live | — | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| Sequence Test | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✓ | ✓ | ✓ | ✓ |
| Runs list | ✓ | — | ✓ | — | — | — | — | — | — | ✓ |
| Selected Run | — | ✓ | ✓ | — | — | — | — | — | — | ✓ |
| Settings | — | — | ✓ | — | — | — | — | — | — | ✓ |

`Unavailable` is a derived presentation, not an operation lifecycle state. Workspaces must not be forced to display states that do not apply.

## 8.4 Minimum-width behavior

At the minimum supported window:

- primary navigation may use compact mode;
- operation-side panels remain visible for active operations and primary setup;
- optional inspectors may collapse behind an explicit, labeled action;
- tables may horizontally scroll only after prioritizing essential columns and permitting column resizing;
- required selectors or persistent section headings, primary actions, critical counters, and direct disabled reasons must remain visible;
- workspace structure must not be replaced with a different mobile navigation model.

## 8.5 Workspace source citation practice

Each following workspace section cites PM, IA, LF, and applicable WF sections. PDS v0.1 is cited only where compatible visual or component evidence is adopted or adapted. Repository evidence is cited only for design-handoff feasibility and is never used to alter behavior.

**Source basis:** *OpenDSS Approved v2 Product Model* §§4–14; *OpenDSS v2 Information Architecture and Screen Inventory* §§1–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§1–20; *OpenDSS Detailed User Workflow Specification* §§7–23 and §§31–43; *OpenDSS Product Design Specification*, Draft v0.1 §§4, 6, 8–12 as supporting design evidence only.

## 8.6 Output-path defaults and persistence

This contract applies only to the output selectors already present in this specification. It does not add output selectors to input, Dataset, sequence, package-import, or other artifact-selection workflows.

| Existing output selector or action | Nonblank standard root |
|---|---|
| Capture > Single Image — Save Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\datasets` |
| Capture > Image Sequence — Save Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\datasets` |
| Capture > Droplet Dataset Capture — Save Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\datasets` |
| Models > Train — Output Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\models` |
| Library Add Model package destination and Export Model destination | `%USERPROFILE%\Documents\OpenDropletSortingSuite\models` |
| Models > Model Test — Output Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\reports` |
| Sort > Live — Save Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\runs` |
| Sort > Sequence Test — Save Location | `%USERPROFILE%\Documents\OpenDropletSortingSuite\runs` |

- Every listed field or action destination is nonblank by default.
- Each operation Start retains its existing unique timestamped/name subfolder and never overwrites an existing output.
- After a valid user selection is accepted and persisted, that location becomes the next default for that same workspace output selector or action.
- If a persisted location is invalid or unavailable, the field visibly falls back to its corresponding standard root before the next operation; it never remains blank or silently retains the invalid path.
- These per-output defaults do not redesign Settings storage, migrate existing artifacts, add a schema, or change any input selector.

---
# 9. Data > Capture design

## 9.1 Common Capture workspace

### Design purpose and user goal

Capture is one live-camera workspace with one stable Camera preview and three collapsible operation sections in the right-side panel. Its design purpose is to keep acquisition visually continuous while exposing Single Image, Image Sequence, and Droplet Dataset Capture without a mode switch. The main user goal is to save source image data without exposing detector, Droplet Crop, model, routing, or internal timing controls.

```text
Data > Capture

┌────────────────────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW                        │ ▸ Single Image               │
│                                            │ ▸ Image Sequence             │
│ Shared across all capture sections         │ ▸ Droplet Dataset Capture            │
└────────────────────────────────────────────┴──────────────────────────────┘
```

### Common design contract

| Contract field | Capture requirement |
|---|---|
| **Major regions** | Shared dark live-camera preview; fixed three-heading Capture panel; independently expandable and scrollable section bodies; optional contextual fault banner above the split region. |
| **Dominant hierarchy** | Camera preview first, three persistent section headings second, expanded section content and primary actions third. |
| **Hardware** | Camera Streaming is required for all capture actions. DAQ and Active Model are never required. |
| **Hardware access** | Camera settings are available only through the shared hardware panel. The operation panel may show a factual Camera summary but no editable Camera fields. |
| **Section behavior** | All headings remain fixed and visible. Sections expand independently; opening one does not close another. With one open body it receives the remaining panel height; with two or three open bodies the remaining height is divided evenly and each body scrolls independently. All sections are collapsed when Capture first opens. |
| **Operation locking** | During Single Image capture/write or from accepted Start through finalization for Image Sequence or Droplet Dataset Capture, the active section is forced open and the other two headings remain visible but disabled. After completion, interruption, or failure, the other headings re-enable and the result section remains expanded until the user collapses it. |
| **Camera unavailable** | Preview remains in place with **Camera unavailable** centered on the dark viewer surface. All section headings and any expanded nonhardware file fields remain visible. |
| **Fault banner** | Above the preview/panel split and below the workspace title. It does not replace the preview geometry or any Capture heading. |
| **Responsive behavior** | Preview receives remaining width. Operation panel SHOULD remain at least 320 logical px. At minimum width, metadata fields may use a denser single column; all three fixed headings remain visible and expanded bodies scroll independently. |
| **Focus behavior** | Preview utilities, if any → Single Image heading and expanded body → Image Sequence heading and expanded body → Droplet Dataset Capture heading and expanded body. Expanding moves focus into the section heading/body sequence; collapsing a section whose body contains focus returns focus to that heading. Disabled headings are skipped. |
| **Mock states** | Camera unavailable; Camera connected but not Streaming; Camera Streaming; conflicting operation; unwritable location; each section Ready; none, one, two, and three sections expanded; active, paused, completed, interrupted, and failed states where applicable. |

The preview MAY show nonpersistent exposure or frame-rate facts supplied by the Camera integration, provided they do not become duplicate settings or imply detector controls.

The per-section specifications below describe each expanded body. They do not replace the shared preview or hide either of the other fixed Capture headings.

## 9.2 Single Image

### Design contract

| Contract field | Specification |
|---|---|
| **Design purpose** | Make one-frame capture immediate and unambiguous. |
| **Main user goal** | Save exactly one full-frame TIFF. |
| **Operation panel** | Heading `Single Image`; optional File Name; Save Location path field with Browse; saved-path confirmation; Capture Image. |
| **Primary action** | **Capture Image** in Ready and again after successful completion. |
| **Secondary actions** | Browse Save Location; Reveal/Open saved location after success; expand or collapse Capture sections while idle. |
| **Required hardware/artifact** | Camera Streaming; writable image location; no conflicting Camera or image-write ownership. |
| **Output** | One TIFF. No Dataset, Image Sequence metadata, Model Test output, Run, classification, or DAQ output. |
| **Direct disabled reasons** | **Another operation is active** → **Camera unavailable** → **Output folder is not writable**. |
| **Applicable presentations** | Unavailable, Ready, transient busy, Completed, Failed. |
| **Next likely action** | Capture another image or expand another Capture section. No mandatory handoff. |

### Ready and completion presentation

```text
SINGLE IMAGE

File Name
[________________________]
Optional; blank uses a timestamp.

Save Location
[ path____________________________ ] [ Browse ]

[ Capture Image ]
```

After a successful write, the panel keeps the fields and action visible and adds:

```text
Saved: <full or elided TIFF path>  [ Reveal ]
```

The saved confirmation uses a completed/check cue but does not transform the whole workspace into a terminal success screen.

### Interaction and state details

- Selecting Capture Image MUST immediately place the button in a busy state, keep Single Image expanded, and disable the other two section headings until the write completes or fails.
- Current Activity MUST show `Capturing Image` until the write succeeds or fails.
- Repeated activation must not create duplicate captures.
- On write failure, the banner must name the file or folder reason where useful and must not display a saved path.
- Focus returns to Capture Image after success so repeated capture remains efficient. On failure, focus moves to the banner heading or recovery action only when the failure requires user action; otherwise it remains in the operation panel.

### Minimum design-review states

1. Camera unavailable with Capture Image disabled.
2. Camera Streaming with timestamp fallback.
3. Custom filename and external writable folder.
4. Momentary busy capture.
5. Successful saved-path confirmation.
6. File-write failure with no false success.
7. Camera occupied by another operation.

## 9.3 Image Sequence

### Design contract

| Contract field | Specification |
|---|---|
| **Design purpose** | Record ordered full-frame source data while preserving a stable live preview and clear Pause/Resume/Stop behavior. |
| **Main user goal** | Create one Image Sequence containing `sequence.json` and numbered TIFF frames. |
| **Ready panel** | Name; Experiment Type; Notes; optional Duration; Save Location; Start Recording. |
| **Running panel** | Status; active elapsed time; Frames; optional writer/preservation status; Pause; Stop. |
| **Paused panel** | `Paused`; frozen active time and frame count; Resume; Stop; explanation that preview continues while writing is stopped. |
| **Completed panel** | Final frame count; stop reason; location; Open in Sequence Viewer; Open in Sequence Test; Start New Recording. |
| **Required hardware/artifact** | Camera Streaming; writable sequence location; global long-running-operation slot. |
| **Output** | `sequence.json` and numbered full-frame TIFF frames. It is not a Run and does not appear in Results. |
| **Direct disabled reasons** | **Another operation is active** → **Camera unavailable** → **Output folder is not writable**. |
| **Applicable presentations** | Unavailable, Ready, Starting, Running, Paused, Stopping, Completed, Interrupted, Failed. |
| **Next likely action** | Open in Sequence Viewer or Open in Sequence Test. |

### Ready presentation

```text
IMAGE SEQUENCE

Name                 [________________________]
Experiment Type      [________________________]
Notes                 [________________________]
Duration              [____________]  optional
Save Location         [ path________________ ] [ Browse ]

[ Start Recording ]
```

Blank Duration is represented by an empty field plus supporting text such as `Continue until Stop`. It must not display `0` as the default.

### Active presentations

```text
RECORDING SEQUENCE
Status          Running
Elapsed         00:03:18
Frames          47,238

[ Pause ]   [ Stop ]
```

Paused:

```text
RECORDING SEQUENCE
Status          Paused
Active time     00:03:18
Frames          47,238

Preview continues. Frame writing is paused.

[ Resume ]  [ Stop ]
```

Stopping:

```text
Finalizing Image Sequence…
New frame writes have stopped.
Writing sequence.json and flushing queued frames.
```

### Interaction and visual behavior

- On Start acceptance, the Image Sequence section remains expanded, applicable fields become read-only, the other two section headings become disabled, and the section transitions to Starting.
- The configuration snapshot is not shown as editable. A concise `Captured with current Camera settings` summary MAY be inspectable.
- Pause must visually preserve the same sequence identity and location. Resume must not look like a new Start.
- The preview remains live during Running and Paused. The Camera section of the hardware panel remains locked; an available DAQ section remains editable.
- Stop is a secondary but persistent action beside Pause/Resume. Once accepted, Pause/Resume disable and the panel displays Stopping.
- Completion actions use normal contextual links. Neither action automatically starts playback or testing until the destination workspace loads the sequence.
- Interrupted and Failed banners must distinguish recoverable `sequence.json`, preserved frames without finalized metadata, and no usable output.

### Keyboard and focus

- `Alt+P` MAY be used as a displayed accelerator for Pause/Resume only while the owning operation panel has focus and no field is being edited.
- Stop must remain available by normal Tab navigation and explicit activation. An unmodified global key must not stop recording.
- When the operation transitions from Ready to Starting, focus moves to the status heading and then follows to Pause/Stop when Running.
- Completion places focus on the result heading; Tab proceeds to Open in Sequence Viewer, Open in Sequence Test, and Start New Recording.

### Minimum design-review states

Camera unavailable; Ready with blank Duration; Ready with Duration; Starting; Running with low and large frame counts; Paused; Stopping; Completed by Duration; Completed by user Stop; Interrupted with recoverable sequence; Failed finalization; DAQ editable while Camera section is locked.

## 9.4 Droplet Dataset Capture

### Design contract

| Contract field | Specification |
|---|---|
| **Design purpose** | Create one model-independent Dataset while visibly separating fixed processing from editable user choices. |
| **Main user goal** | Record full frames and save one initially Unlabeled Droplet Crop per detected droplet. |
| **Ready panel** | Dataset Name; Experiment Type; Notes; optional Duration; Save Location; concise fixed-processing explanation; Start Droplet Dataset Capture. |
| **Running panel** | Status; active elapsed time; Full Frames; Detected Droplets; Droplet Crops; Pause; Stop. |
| **Paused panel** | Paused status; frozen active counters; explanation that preview continues while frame writing, detection, and Droplet Crop creation stop; Resume; Stop. |
| **Completed panel** | Final counts; Dataset location; Open in Label; Open Folder; Start New Droplet Dataset Capture. |
| **Required hardware/artifact** | Camera Streaming; loadable fixed qualified processing configuration; writable Dataset location; global operation slot. No model or DAQ. |
| **Output** | `dataset.json`, full-frame Image Sequence, and one 64 × 64 grayscale PNG Droplet Crop per completed detection, initially Unlabeled. |
| **Direct disabled reasons** | **Another operation is active** → **Camera unavailable** → **Processing configuration unavailable** → **Output folder is not writable**. |
| **Applicable presentations** | Unavailable, Ready, Starting, Running, Paused, Stopping, Completed, Interrupted, Failed. |
| **Next likely action** | Open in Label. |

### Ready presentation

```text
DROPLET DATASET CAPTURE

Dataset Name          [________________________]
Experiment Type       [________________________]
Notes                  [________________________]
Duration               [____________]  optional
Save Location          [ path________________ ] [ Browse ]

Processing
Fixed qualified detection and Droplet Crop processing.
The effective configuration is recorded with the Dataset.

[ Start Droplet Dataset Capture ]
```

The fixed-processing explanation MUST be read-only text or an information disclosure. It MUST NOT be styled as an expandable settings section and must not contain detector or crop controls.

### Running presentation

```text
CAPTURING DATASET
Status                Running
Elapsed               00:12:44
Full Frames           183,442
Detected Droplets     2,341
Droplet Crops         2,338

[ Pause ]   [ Stop ]
```

If detection and persisted crop counts differ transiently because of queued writes, labels must make that distinction factual. The UI must not imply lost data until the persistence state reports a fault.

### Interaction and visual behavior

- Droplet Dataset Capture MUST never show Active Model, Predicted Class, Class Score, Decision, Hit Class, Decision Boundary controls, Observed Route, or DAQ controls.
- Every newly persisted crop begins as Unlabeled. No automatic label, scientific acceptance, or candidate rejection appears.
- Pause stops full-frame writes, detection, and Droplet Crop creation while the live preview continues.
- The Camera panel section locks from Starting through Stopping. The DAQ section remains independently editable when available and idle.
- Completion provides Open in Label as the primary contextual action. It may preselect the Dataset but must not define classes or begin labeling automatically.
- Interrupted messaging must report whether a readable `dataset.json` was finalized, whether full frames/crops remain, and whether Open Dataset or only Open Folder is valid.

### Minimum design-review states

No Active Model; Camera unavailable; fixed processing unavailable; Ready; Starting; Running with counters; Paused; Stopping; Completed; Interrupted with recoverable Dataset; Interrupted with files only; write failure; camera disconnect; Open in Label handoff.

**Source basis:** *OpenDSS Approved v2 Product Model* §§7.1, 8, 11–14 and D-014, D-016, D-019; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.2–4.4, and §5; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §6, §16, §17, and §18; *OpenDSS Detailed User Workflow Specification* §§11–13 and §§31–34 as amended to remove editable detection and Droplet Crop controls; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–10 as adapted visual/component evidence.

---

# 10. Data > Label design

## 10.1 Design purpose and user goal

Label is a high-throughput Dataset review workspace. Its purpose is to let the scientist define two or three classes, inspect an adaptive/virtualized Droplet Crop collection, assign or change Labels, Skip, Remove from Dataset, restore, filter, and Undo while keeping Class ID, Class Name, selection, focus, and crop state unambiguous.

The design must present factual Image Counts without class-balance warnings or scientific quality judgments.

## 10.2 Layout

```text
Data > Label

DATASET HEADER: <Dataset name/path>                            [ Open Dataset ]
FILTERS: [ Class ▼ ] [ State ▼ ] [ Search/ID if supported ]
OPTIONAL CONTEXTUAL FAULT BANNER

┌─────────────────────────────────────────────────┬──────────────────────────┐
│ VIRTUALIZED DROPLET CROP COLLECTION             │ SELECTED CROPS INSPECTOR │
│                                                 │                          │
│ [crop] [crop] [crop] [crop] [crop]              │ Large preview            │
│ [crop] [crop] [crop] [crop] [crop]              │ Selected: <count>        │
│                                                 │ Metadata                  │
│ Selection, focus, class, and state are separate │ Label actions            │
│                                                 │ Skip / Remove / Restore  │
├─────────────────────────────────────────────────┼──────────────────────────┤
│ IMAGE COUNTS                                    │ CLASSES                  │
│ Class 0 — <name>        <count>                 │ 0 [ name____________ ]   │
│ Class 1 — <name>        <count>                 │ 1 [ name____________ ]   │
│ Class 2 — <name>        <count>                 │ 2 [ name____________ ]   │
│ Unlabeled / Skipped / Removed                   │ [ Use in Train ]         │
└─────────────────────────────────────────────────┴──────────────────────────┘
```

In the maximized layout, Image Counts and Classes may occupy a lower summary strip or the lower part of the inspector. The crop collection remains the dominant region.

The Label header MUST size itself to its current visible content. When header content contracts, the header MUST collapse to the resulting content height and leave no residual blank or reserved whitespace below it.

## 10.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Assign correct user-chosen Labels to selected Droplet Crops efficiently. |
| **Major regions** | Dataset header; filters; virtualized crop collection; selected-crop inspector; Image Counts; class definition/name controls; contextual fault banner. |
| **Dominant hierarchy** | Crop collection, current selection, and available class actions. |
| **Operation-side-panel content** | Selected preview/count, metadata, Class 0/1/2 actions, Skip, Remove from Dataset, Restore when applicable, Undo, and contextual Use in Train. |
| **Primary action** | Class assignment for a valid selection. In Empty, Open Dataset. |
| **Secondary actions** | Relabel; bulk label; Skip; Remove from Dataset; Restore; Undo; filters; edit Class Names; Use in Train. |
| **Required artifact/hardware** | Readable v2 `dataset.json` and referenced crops; selected Dataset not write-locked by Training or Model Test. No hardware. |
| **Output** | Atomic updates to class definitions, Class Names, Labels, and crop states in the same `dataset.json`. |
| **Direct disabled reasons** | **Dataset is in use by Training/Model Test** → **No dataset selected** → **No Droplet Crop selected** → **Number of Classes not selected**. Missing crop/schema/write reasons appear at their affected scope. |
| **Fault banner** | Below Dataset header; spans collection and inspector. Used for load or persistence failure. |
| **Applicable presentations** | Empty, Unavailable, Ready, Failed. Dataset lock is Unavailable for mutation but may retain readable content. |
| **Next likely action** | Use in Train with current Dataset preselected. |

## 10.4 Empty and class-definition states

Empty:

```text
No Dataset selected
Open a supported OpenDSS v2 dataset.json.

[ Open Dataset ]
```

Dataset with no class definition:

```text
Number of Classes
( ) 2 Classes     ( ) 3 Classes

Class IDs will be created as 0 and 1, or 0, 1, and 2.
Class count becomes fixed after the first Label is assigned.
```

No recommendation, preselection, or quality language should imply which class count the scientist ought to choose. After class creation, Class IDs remain immutable and Class Names remain editable.

## 10.5 Crop collection and virtualization

- The collection MUST virtualize image loading and component creation so large Datasets remain responsive.
- Thumbnail aspect treatment must preserve the complete 64 × 64 crop without unintended clipping.
- Loading placeholders use neutral skeletons or reserved boxes, not false image content.
- A missing crop uses a factual missing-file tile with crop ID/path context and must not be visually confused with Removed.
- The default grid cell MUST be exactly 185 × 185 logical px and square. A visual-density control MAY offer other sizes, but initial/default presentation remains 185 × 185 logical px and crop order/state must not change.
- Selection count remains visible in the inspector.
- Standard click selects one and establishes the Shift anchor; Ctrl-click toggles an individual crop; Shift-click selects the contiguous range from the current anchor to the clicked crop in the current visible filtered order only.
- On every active-filter change, selected crops that become hidden are cleared from selection and excluded from any batch assignment, and the Shift anchor is reset. Filtering never changes persisted Labels or crop states.

## 10.6 Thumbnail visual anatomy

Each Dataset thumbnail MUST reserve separate layers:

1. crop image with an exactly 6 logical-px crop border;
2. class badge containing Class ID and compact Class Name when Labeled;
3. state badge for Unlabeled, Skipped, Removed, or missing file;
4. selection fill/indicator;
5. keyboard-focus ring;

An assignment through `Empty`, `Single`, `MoreThanOne`, or `Exclude` applies atomically to the full current selection. `Exclude` assigns the retained `Skipped` state; it never deletes or removes a crop. Only after the complete selection is persisted successfully does Label advance to the first item after the highest selected item in the current visible filtered order. If the assignment includes the final filtered item, that final item remains selected and the selection never wraps. A failed atomic persistence operation changes none of the selected crops and does not advance.
6. optional multi-selection check indicator.

The 6 logical-px crop border is its own visual layer. Selection and keyboard focus are separate orthogonal layers and MUST NOT replace, resize, merge into, or be represented solely by the crop border.

Recommended treatment:

- **Unlabeled:** neutral border and `Unlabeled`/open-circle marker.
- **Labeled:** class token plus `0`, `1`, or `2` badge and check marker.
- **Skipped:** neutral muted badge labeled `Skipped`; image remains fully inspectable.
- **Removed:** subdued overlay plus X and `Removed`; selection and focus remain visible above the overlay.
- **Selected:** neutral dark selection surface and check/structural marker, independent of class identity and crop border. Selection MUST NOT use the blue/navy viewer treatment or imply a class.
- **Focused:** high-contrast focus ring outside selection/class layers.

## 10.7 Class assignment and crop actions

| Action | Required result and presentation |
|---|---|
| **Class 0 / 1 / 2** | Sets selected crop entries to Labeled with the chosen immutable Class ID. The action label includes current Class Name where space permits. |
| **Relabel** | Uses the same class action. Previous class badge changes after persistence succeeds. |
| **Skip** | Sets selected crops to Skipped with no training-eligible Label. |
| **Remove from Dataset** | Sets selected crops to Removed; files and entries remain on disk. A direct confirmation is not required if Undo and Restore are clearly available, but the action must state scope for multi-selection. |
| **Restore** | Returns selected Removed crops to Unlabeled, not to their prior Label unless the authoritative workflow changes. |
| **Undo** | Reverts the most recent Label-workspace edit in the current session and persists the reverted Dataset state atomically. |

For bulk actions, the control should state selection scope, for example `Assign Class 1 to 24 crops` in an accessible description or confirmation line.

## 10.8 Autosave and save-error feedback

There is no Dataset Save button. Each completed labeling, Class Name, Skip, Remove, Restore, or Undo command is persisted atomically.

The UI SHOULD use a small factual save-state line in the Dataset header:

- `Saved` after canonical persistence;
- `Saving…` during the atomic operation;
- `Change not saved` when persistence fails.

On failure:

- the prior canonical JSON remains authoritative;
- the UI must not show the attempted state as committed without a clear unsaved/error treatment;
- the workspace shows `Error`, logs the write/permission detail, and retains only directly useful recovery actions;
- Retry may be offered when the same command can be safely reattempted.

This save-state line is not a notification center and should not produce repeated transient popups.

## 10.9 Dataset lock behavior

When Training or Model Test uses the selected Dataset:

- crop content and counts may remain visible;
- mutating controls disable with **Dataset is in use by Training** or **Dataset is in use by Model Test**;
- filters and nonmutating inspection remain available;
- another unlocked Dataset may be opened and edited;
- the lock is visually distinct from a read-only file or failed save.

## 10.10 Keyboard and focus behavior

Recommended high-throughput shortcuts, active only when focus is in the crop collection/inspector and not in a text field:

| Key | Action |
|---|---|
| `1`, `2`, `3` | Assign Class ID 0, 1, or 2 respectively. `3` is inactive for two-class Datasets. |
| `S` | Skip selected crop(s). |
| `X` | Remove selected crop(s) from Dataset. |
| `R` | Restore selected Removed crop(s). |
| `Ctrl+Z` | Undo last Label-workspace edit. |
| Arrow keys | Move focused crop. |
| `Shift` + Arrow | Extend selection. |
| `Space` | Toggle selection of focused crop; it must not trigger a global operation. |
| `Enter` | Move focus to or open the selected-crop inspector without changing Label. |

Shortcut hints SHOULD appear in tooltips, an in-workspace shortcut help panel, and accessible descriptions. Editing a Class Name suppresses all single-letter and number shortcuts until the field commits or loses focus.

## 10.11 Resizing and minimum width

- Crop grid and inspector use a splitter.
- Inspector SHOULD default to 340–440 logical px and may collapse to a labeled `Selected Crops` action.
- At minimum width, Image Counts and Classes may move into tabs or collapsible sections inside the inspector, but the crop collection and primary class actions remain available.
- The grid must recompute columns without changing crop order.
- Collapse/expand must not clear selection or move keyboard focus to an invalid target.
- The Label header must recompute to its visible content height and must not retain whitespace when its content contracts.

## 10.12 Minimum mock-data states

1. No Dataset.
2. Unsupported schema.
3. Dataset with no classes.
4. Two-class empty Dataset.
5. Three-class partially labeled Dataset.
6. Mixed Unlabeled, Labeled, Skipped, and Removed crops.
7. Multi-selection spanning different prior states.
8. Missing Droplet Crop file.
9. Dataset locked by Training.
10. Dataset locked by Model Test.
11. Autosave in progress.
12. Atomic save failure.
13. Large virtualized Dataset with at least 100,000 mock entries.
14. Filter-empty state.
15. Class Name editing at 200% scaling.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.2, §§8, 11, 13–15 and D-012, D-019; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.5, 5, and 6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §7, §16, §17, and §18; *OpenDSS Detailed User Workflow Specification* §14 and §§31–34, 38–43; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§8–10 as adapted thumbnail, keyboard, responsive, and accessibility evidence.

---

# 11. Data > Sequence Viewer design

## 11.1 Design purpose and user goal

Sequence Viewer is a hardware-independent still-frame inspection workspace. Its purpose is to present recorded full frames in order and make frame navigation, direct seek, zoom, and pan clear without implying processing, classification, or DAQ output.

## 11.2 Layout

```text
Data > Sequence Viewer                                      [ Open Sequence ]
OPTIONAL CONTEXTUAL FAULT BANNER

┌────────────────────────────────────────────────────────────────────────────┐
│ DARK IMAGE VIEWER                                                         │
│                                                                            │
│                              <current frame>                               │
│                                                                            │
├────────────────────────────────────────────────────────────────────────────┤
│ Frame <current> of <total>                                                 │
│ [ Previous ] [ Next ]  Frame [________]                                   │
│ [ Zoom - ] [ Zoom + ] [ Fit ] [ 1:1 ]                                    │
└────────────────────────────────────────────────────────────────────────────┘
```

A compact metadata or source strip MAY show the sequence name and location. Acquisition metadata is secondary and must not crowd the viewer.

## 11.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Inspect a recorded Image Sequence visually. |
| **Major regions** | Sequence header/open action; dark image viewer; frame/total status; previous/next and direct seek; zoom/pan/Fit/1:1 controls; optional fault banner. |
| **Dominant hierarchy** | Current frame, then frame position and navigation controls. |
| **Operation-side panel** | Not required as a permanent right panel. In the maximized layout, optional sequence details may occupy a collapsible inspector. |
| **Primary action** | Open Sequence when Empty; Next Frame when a sequence is loaded. |
| **Secondary actions** | Previous Frame; direct seek; Fit; 1:1; zoom; pan; Open Sequence; direct Open in Sequence Test when explicitly provided for the loaded sequence. |
| **Required artifact/hardware** | Standalone v2 `sequence.json`, Dataset-referenced sequence, or Run-referenced sequence. No Camera, DAQ, model, or training environment. |
| **Output** | No scientific artifact and no DAQ output. |
| **Direct disabled reasons** | **No sequence selected**; direct unreadable sequence or missing-frame reason. |
| **Fault banner** | Below sequence header and above viewer; missing frame may also use an in-view factual overlay. |
| **Applicable presentations** | Empty, Ready, Failed. Frame navigation does not occupy the global long-running-operation slot. |
| **Next likely action** | Continue review or open the same sequence in Sort > Sequence Test. |

## 11.4 Empty, unavailable file, and missing-frame treatment

Empty state retains the viewer canvas with:

```text
No Image Sequence selected
[ Open Sequence ]
```

On a `sequence.json` open failure, the previous valid sequence remains selected when possible; the workspace shows `Error` and logs the technical detail.

When an individual frame is missing or unreadable, the viewer skips it silently and continues to the next readable frame. It never presents a decorative substitute as source data.

## 11.5 Playback behavior

- Previous/Next and direct seek navigate readable frames without automatic progression.
- Pause freezes the current frame.
- Previous and Next move exactly one frame when visual playback is not advancing.
- Timeline scrubbing seeks to the selected frame and updates current/total text.
- Reaching the final frame produces Completed presentation while retaining stepping, scrubbing, and replay.
- Frame navigation affects only visual playback. It must be labeled so it cannot be mistaken for Sequence Test processing rate.
- Zoom and pan affect presentation only and never modify source files.

## 11.6 Viewer controls and overlays

Viewer overlays SHOULD use dark translucent surfaces and automatically avoid covering critical image regions where practical. They must remain accessible without hover. A small zoom value and frame count may remain visible during playback, but operation-unrelated metadata should not obstruct the frame.

## 11.7 Keyboard and focus behavior

| Key | Action |
|---|---|
| `Left` / `Right` | Previous/next readable frame when viewer focus is active. |
| `Left` / `Right` | Previous/Next Frame. |
| `Home` / `End` | First/last frame. |
| `Page Up` / `Page Down` | Larger backward/forward seek by an implementation-defined visible increment. |
| `F` | Fit frame. |
| `1` | 1:1 display. |
| `+` / `-` | Zoom in/out. |

The timeline thumb must be keyboard operable and expose current frame, total frames, and percentage position. Focus does not move on every frame advance.

## 11.8 Responsive behavior and mock states

- The viewer must not collapse below the minimum usable image area. Optional details collapse before frame-navigation controls.
- Controls may wrap into two rows at minimum width, preserving order and labels.
- Timeline retains the largest horizontal share.
- Mock states: no sequence; valid standalone sequence; Dataset sequence; Run sequence; first/middle/final frame; one-frame sequence; very large frame count; silently skipped missing middle frame; unreadable sequence; 200% scaling; direct Open in Sequence Test handoff.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.3, §8, and §12; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.6, and §5; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §8 and §18; *OpenDSS Detailed User Workflow Specification* §18 and applicable file/recovery requirements; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–10 as adapted viewer and accessibility evidence.

---

# 12. Models > Train design

## 12.1 Design purpose and user goal

Train presents one qualified, fixed-configuration model-training workflow. Library owns model creation and import; Train consumes one selected Library-defined model read-only for identity, architecture, and initialization. Train's purpose is to make Dataset eligibility, two-class or three-class structure, selected model identity, requested/effective compute device, Output Location, progress, factual metrics, atomic package creation, and activation clear without exposing training hyperparameters or scientific quality gates.

## 12.2 Layout by phase

Ready:

```text
Models > Train

┌──────────────────────────────────────────────┬────────────────────────────┐
│ DATASET SUMMARY                              │ TRAINING SETUP             │
│ Dataset: <name> [ Select Dataset ]           │ LIBRARY MODEL             │
│ Classes: 2 or 3                              │ Library Model [ Select ]  │
│ Class 0 — <name>: <eligible count>           │ Name <read-only>          │
│ Class 1 — <name>: <eligible count>           │                           │
│ Class 2 — <name>: <eligible count>           │ Architecture <read-only>  │
│                                              │ Starting Weights <read-only>│
│                                              │ Compute Device [ GPU/CPU ]│
│                                              │ Output Location [___] […] │
│                                              │                           │
│                                              │ [ Start Training ]        │
└──────────────────────────────────────────────┴────────────────────────────┘
```

Running:

```text
┌──────────────────────────────────────────────┬────────────────────────────┐
│ TRAINING METRICS                             │ TRAINING STATUS            │
│ Training/Validation Loss plot                │ Status        Running     │
│ Validation Accuracy plot                     │ Device        <GPU/CPU>   │
│                                              │ Elapsed       <time>      │
│                                              │ Epoch         <n> of <n> │
│                                              │ Overall       <progress>  │
│                                              │ Estimated     <time>      │
│                                              │ [ Stop Training ]         │
└──────────────────────────────────────────────┴────────────────────────────┘
```

Completed after automatic save:

```text
Training completed
Overall results table
Per-class results table
Confusion matrix when generated
Saved path: <confirmed path>
Active Model: <name>
[ Open in Model Test ]  [ Open in Library ]
```

## 12.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Train one Library-defined model identity into a new technically completed two-class or three-class Model Package without mutating any source package. |
| **Major regions** | Dataset summary; selected Library model identity; requested/effective device; Output Location; Training status; plots/progress; completion results; minimal Error/Retry Save. |
| **Dominant hierarchy** | Before Start: Dataset, read-only Library model identity/architecture/initialization, Compute Device, and Output Location. During Training: progress and two live plots. After completion: atomically saved results and Active Model confirmation. |
| **Operation-side panel** | Dataset selector/summary, Library model selector with read-only identity facts, Compute Device, Output Location, Start/Stop, and status. |
| **Primary action** | Start Training in Ready; Stop Training in Running; Retry Save only after automatic-save failure. |
| **Secondary actions** | Select Dataset; select an existing Library-defined model; select Compute Device; browse Output Location; Open in Model Test; Open in Library; Diagnostics after technical failure. |
| **Required artifact/hardware** | Labeled Dataset with readable Labeled crops; one selected Library-defined unique model identity with supported architecture and approved Starting Weights; training runtime; writable temporary/final locations; global operation slot. No Camera or DAQ. |
| **Output** | A new Model Package containing `metadata.json`, `checkpoint.pth`, and `model.onnx`. Successful atomic save registers the new package and makes it Active Model; it never overwrites or mutates a source package. |
| **Direct disabled reasons** | At navigation: **Training environment unavailable. Open Settings > Training Environment.** After navigation is available: **Another operation is active** → **No dataset selected** → **No Labeled Droplet Crops** → **Required Droplet Crop is missing** → **No Library model selected** → **Compute Device unavailable** → **Output location is not writable**. The navigation gate uses the shared authoritative Training Environment diagnosis. |
| **Fault banner** | Below workspace heading, above setup/metrics. It identifies input, runtime, process, or write failure and available Diagnostics/output. |
| **Applicable presentations** | Empty, Unavailable, Ready, Starting, Running, Stopping, Completed, Interrupted, Failed. No Pause. |
| **Next likely action** | Open in Model Test after successful save. |

## 12.4 Dataset summary and class presentation

- The selected Dataset summary displays Dataset name/path, class count, immutable Class IDs, current Class Names, and eligible Labeled crop counts.
- Unlabeled, Skipped, and Removed counts MAY be shown as factual secondary information but must not receive warning styling.
- No class-balance warning, suitability score, or minimum-per-class recommendation appears.
- A missing required Droplet Crop is a technical file error, not a Dataset quality judgment.

## 12.5 Library model identity

Train has no Architecture, Starting Weights, or Model Name editor. Those values come read-only from the selected Library-defined model identity. The only supported architectures are:

```text
MobileNetV3-Small
EfficientNet-B0
```

Library Add Model requires a nonblank unique Name, one of those two architectures, and one of exactly two Starting Weights choices:

- `ImageNet`: the fixed bundled local ImageNet checkpoint for the selected architecture;
- `Pretrained`: a second fixed bundled local checkpoint for the selected architecture, distinct from `ImageNet`. This Starting Weights selection is not itself a Library entry; the installer-owned ready-to-run packages are separate complete Model Packages under §2.1.1.

Both labels remain visible for both supported architectures. Changing Architecture refreshes both choices to that architecture's own two bundled checkpoints. Each choice is enabled only when its corresponding bundled artifact is locally valid. Add Model requires Name, Architecture, and one of the two choices. Add Model MUST NOT download weights, present a network action, infer or substitute another artifact, or use a network fallback. Train must not infer, substitute, download, or edit Name, Architecture, or Starting Weights.

Train shows every Library model as selectable and trains the selected model. It performs no Library-model compatibility filtering and shows no compatibility warning, disabled compatibility reason, or `No compatible Library models are available` state. When Library contains no models, the exact empty message is **No Library models are available**.

Retraining an already-trained Library model always begins with a new unique Name defined through Add Model and produces a new Library entry/package. The prior package is read-only initialization evidence and remains intact and protected; retraining never overwrites or mutates it.

The panel does not show qualified-configuration, split, or seed helper copy. There is no Advanced Training Parameters heading, disclosure, button, tab, dialog, or placeholder.

## 12.6 Compute-device selection and display

Before Start, the user selects the requested device:

- `GPU`;
- `CPU`.

During and after Training, the requested and effective execution device are shown and recorded. Existing qualified GPU-to-CPU fallback behavior remains factual; the selected Library model identity, architecture, and Starting Weights remain unchanged.

## 12.7 Metrics presentation

- Metrics use neutral scientific data treatment, not red/green pass-fail encoding.
- Training Loss, Validation Loss, Validation Accuracy, Per-Class Validation Accuracy, Macro F1, elapsed time, epoch progress, overall progress, estimated remaining time, and device are displayed as available.
- Metric tiles must label units and data scope.
- Two minimal live plots are required: Training/Validation Loss and Validation Accuracy. They do not turn Train into a general analytics dashboard.
- Technical console output remains in Diagnostics, not the normal workspace.

## 12.8 Stop, completion, and saving

- Stop Training becomes available after Start is accepted and remains until processing stops.
- Stopping states that processing is ending and temporary output is being finalized.
- User-stopped or failed Training does not show normal Model Package completion.
- Low accuracy, model collapse, or one dominant Predicted Class does not block automatic save when the technical artifact contract is satisfied.
- Automatic save begins after Training completes and uses the Library-defined unique Name and the pre-start Output Location.
- Save failure shows `Error`, retains completed temporary artifacts, exposes Retry Save, does not publish or activate the new package, and leaves every source identity/package intact.
- Successful save atomically writes the newly named package, then updates Library and the global Active Model header. The selected Dataset and every source package remain unchanged.

## 12.9 Keyboard, resizing, and mock states

- Tab order follows Dataset → Library Model → Compute Device → Output Location → Start; while Running it begins at status/progress and reaches Stop.
- Compute Device options use arrow-key navigation.
- Metrics must remain readable by screen readers in a stable label/value order and should not announce every high-frequency update; summarized progress announcements are sufficient.
- At minimum width, metrics may form one scrollable column while the status panel remains visible. The Automatic Save region must not appear below an unbounded metric canvas.
- Required mocks: no Dataset; no Labeled crops; no Library model; MobileNetV3-Small Ready/CPU; EfficientNet-B0 Ready/GPU; retraining into a distinct new Name with source protected; runtime unavailable; Starting; Running early/mid/late; Stopping; Completed with low metrics; save write failure; atomically saved Active Model; Interrupted; Failed with Diagnostics action.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.4, §§8, 11–13 and D-003, D-012, D-015; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1 and 4.7; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §9, §16, and §17; *OpenDSS Detailed User Workflow Specification* §15 and §§31–34 as amended to remove all Advanced Training Parameters; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–12 as adapted metrics, QDS, and accessibility evidence.

---

# 13. Models > Model Test design

## 13.1 Design purpose and user goal

Model Test is an optional observational workspace. Its purpose is to use the Active Model with one compatible labeled Dataset, run inference over eligible Labeled Droplet Crops, and review factual classification measurements without assigning approval. Its navigation destination uses the same authoritative Training Environment diagnosis as Train and is not activatable until that diagnosis passes. When unavailable, its explanation is **Training environment unavailable. Open Settings > Training Environment.**

`UAT-MODEL-005` treats this dataset-wide operation as Dataset Validation for backend throughput. The visible, non-engineering-facing workspace name remains exactly `Model Test`. Its one input remains a structured, labeled OpenDSS Dataset selected through the existing Dataset workflow; it does not add arbitrary image-file or folder input. Automatic multi-image batches are an internal processing detail and do not change the visible input model, scientific meaning, artifacts, metrics, or observational status.

## 13.2 Layout

Ready:

```text
Models > Model Test

┌──────────────────────────────────────────────┬────────────────────────────┐
│ SELECTED ARTIFACTS                           │ TEST SETUP                 │
│ Active Model: <name>                         │ Compatibility  <fact>     │
│ Classes: <2 or 3>                            │ Device         <GPU/CPU>  │
│ Dataset: <name> [ Select Dataset ]           │ Output Location [___][…] │
│ Eligible Labeled Crops: <count>              │                           │
│                                              │ [ Start Model Test ]      │
└──────────────────────────────────────────────┴────────────────────────────┘
```

Running:

```text
Model Test running
Device      <GPU or CPU>
Processed   <n> of <total>
Progress    <determinate bar>

[ Stop Model Test ]
```

Completed:

```text
Overall Accuracy        <value>

Per-Class Accuracy
Class 0 — <name>        <value>
Class 1 — <name>        <value>
Class 2 — <name>        <value>

Confusion Matrix
<2 × 2 or 3 × 3 data table>

[ Open Model Test Summary ]  [ Open Predictions CSV ]
[ Open Output Folder ]         [ Start Another Model Test ]
```

## 13.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Measure a model's classification behavior on a technically compatible labeled Dataset. |
| **Major regions** | Model/Dataset summaries; compatibility and device status; output selection; progress; results; confusion matrix; output actions; contextual fault banner. |
| **Dominant hierarchy** | Before Start: artifact compatibility. During Running: progress. After completion: Overall Accuracy, Per-Class Accuracy, and confusion matrix. |
| **Operation-side panel** | Artifact selection, compatibility, planned/actual device, output path, Start/Stop, progress. |
| **Primary action** | Start Model Test in Ready; Stop Model Test in Running; Open Predictions CSV or Open Model Test Summary in Completed, with one selected as the local primary according to the approved implementation. |
| **Secondary actions** | Select Dataset; Set Active in Library; Open Model Test Summary; Open Predictions CSV; Open Output Folder; Start Another Model Test. |
| **Required artifact/hardware** | A Training Environment that has passed the same authoritative readiness check used by Train; readable 2- or 3-class Model Package; readable labeled Dataset with same class count; readable Labeled crops; writable output; global operation slot. No hardware. GPU optional; CPU fallback. |
| **Output** | `model_test_summary.json` and `predictions.csv`. It is not a Run and is not listed under Results. |
| **Direct disabled reasons** | At navigation: **Training environment unavailable. Open Settings > Training Environment.**, using the same diagnosis owner and presentation as Train. After navigation is available: **Another operation is active** → **No Active Model** → **No dataset selected** → factual class-count mismatch → **No Labeled Droplet Crops** → **Required Droplet Crop is missing** → **Output folder is not writable**. GPU absence is never a blocker. |
| **Fault banner** | Below workspace heading and above selection/results. |
| **Applicable presentations** | Empty, Ready, Starting, Running, Stopping, Completed, Interrupted, Failed. No Pause. Training Environment unavailability is handled by the navigation gate and never opens or replaces the workspace with a dedicated Failed/Unavailable page. |
| **Next likely action** | Review/open output or run another test. Active Model remains unchanged. |

## 13.4 Compatibility presentation

Compatibility is factual and scoped to the selected pair:

- `Compatible — 2 classes` or `Compatible — 3 classes` when class counts match and artifacts are readable;
- a direct mismatch sentence when they do not;
- no `approved`, `validated`, `passed`, `failed validation`, or suitability label.

Required mismatch wording follows this pattern:

> The selected model has 2 output classes, but the selected Dataset defines 3 classes.

The source Dataset used for Training may be selected without warning or confirmation.

## 13.5 Device presentation

Training and Model Test/Dataset Validation execute through the local application-owned Python/PyTorch backend provisioned at `%LOCALAPPDATA%\OpenDSS\training-venv-gpu`. Model Test/Dataset Validation MUST NOT use the C++ ONNX Runtime inference path.

The Python/PyTorch Model Test backend uses automatic GPU acceleration when compatible and CPU fallback otherwise. The UI shows:

- planned device before Start;
- actual device while Running and in the summary;
- CPU fallback as normal operational status, not degradation, when it satisfies the runtime contract.

There is no device selector.

### 13.5.1 Automatic batching and throughput

Dataset Validation MUST process eligible images through automatic multi-image Python/PyTorch batching whenever the qualified model/runtime/provider supports a batch larger than one.

- Batch size is not a user setting. The implementation automatically selects the largest qualified batch that fits the selected device's currently available memory, capped by the remaining eligible images.
- The implementation MAY use a bounded decode/preprocess/inference/persistence pipeline to keep the selected device supplied. It MUST NOT use unbounded queues, retain the whole Dataset in memory merely for throughput, or change source-image order.
- Every image retains its own factual Class Scores, Predicted Class, source identity, and output row. Batching MUST NOT average, merge, omit, or reorder per-image results.
- `Processed` counts only images whose predictions have been durably finalized. Progress advances by the number of images in each completed batch and remains determinate against the original eligible-image total.
- Each completed batch is an atomic persistence checkpoint. Predictions from a partially completed or failed batch are not published; every earlier completed batch remains recoverable under the existing hash, atomic-write, partial-summary, and partial-CSV contracts.
- Stop prevents another batch from starting. The current in-flight batch may finish and checkpoint atomically before Stopping completes; no later batch begins.
- If the selected batch cannot be allocated before inference, the implementation lowers the batch size and retries without publishing output for that attempt. A batch-size-one allocation/inference failure uses the existing truthful Failed/Interrupted behavior; it MUST NOT introduce a network fallback.
- The exact numeric batch size is an implementation/runtime fact, not a product constant. Planned and actual device presentation remains as specified in §13.5; no batch-size control or performance-tuning panel is added.

### 13.5.2 Executed-artifact provenance

Python/PyTorch Model Test executes `checkpoint.pth`, so its trusted Model Package metadata MUST declare the exact `checkpoint_sha256`. New Model Test summaries use schema `opendss.model_test.v3`; `active_model` records the executed `checkpoint_sha256` and the exact `metadata_sha256`. It MUST NOT attribute Python/PyTorch results to `model.onnx` bytes.

Legacy trusted local packages that contain `checkpoint.pth` but lack `checkpoint_sha256` are migrated automatically: OpenDSS computes the checkpoint SHA-256 and atomically adds the declaration before Model Test can execute. A declared mismatch, missing checkpoint, unreadable checkpoint, or failed atomic migration blocks Model Test with truthful technical failure. Existing `opendss.model_test.v2` summaries remain readable; all new Python/PyTorch Model Test output is v3. This migration and provenance are automatic internal behavior with no new visible UI, setting, or user action.

Live and Sequence Test execute `model.onnx`; their provenance remains the exact ONNX SHA-256 plus metadata SHA-256. The Model Test schema change does not alter their runtime, package selection, or provenance contract.

## 13.6 Results treatment

- Overall Accuracy and Per-Class Accuracy use neutral metric tiles or label/value groups.
- The confusion matrix is a data table with Class ID and Class Name on both axes. It must support two or three classes without empty placeholder rows/columns.
- Scientific results must not receive pass/fail color thresholds.
- Output actions clearly identify the Model Test Summary, `Predictions CSV`, and output folder.
- Class output is described as Class Scores, never Confidence.
- No integrated misclassified-image browser is introduced.

## 13.7 Stop and failure behavior

- Stop starts no further batch. An already in-flight batch may finish and checkpoint under §13.5.1; finalization includes only technically representable completed-batch output.
- Interrupted and Failed banners state whether a partial summary or CSV exists.
- If output is incomplete, the workspace must not present full-completion metrics as canonical.
- A model or Dataset parse failure leaves the previous valid selection unchanged when possible.

## 13.8 Keyboard, resizing, and mock states

- Artifact selectors occur first in Tab order, followed by output path and Start.
- Confusion matrix cells expose row/column headers and values to assistive technology.
- At minimum width, artifact summaries stack above the operation panel; completed metrics may scroll vertically, but output actions remain reachable.
- Required mocks: disabled Model Test navigation while the shared Training Environment check is unavailable; no artifacts; model only; Dataset only; 2-vs-3 mismatch; no Labeled crops; GPU planned; CPU fallback; Starting; Running; Stopping; Completed two-class; Completed three-class; Interrupted with partial output; processing failure; write failure; proof that Active Model header does not change. No dedicated installed-runtime Failed/Unavailable workspace mock exists.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.5, §§11–13 and D-004, D-011, D-012, D-013; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1 and 4.8; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §10, §16, and §17; *OpenDSS Detailed User Workflow Specification* §16 and §§31–34 as amended for automatic GPU acceleration with CPU fallback; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–12 as adapted metrics, table, and accessibility evidence.

---

# 14. Models > Library design

## 14.1 Design purpose and user goal

Library is the local master-detail workspace for valid OpenDSS v2 Model Packages. Its purpose is to distinguish the selected Model Package from the global Active Model and to expose technical package actions without adding approval, candidate, promoted, rejected, archived, or certified states.

## 14.2 Layout

```text
Models > Library

┌──────────────────────────────────────────────┬────────────────────────────┐
│ MODEL PACKAGES                               │ SELECTED MODEL             │
│                                              │                            │
│ <Name> | <Architecture> | <2 Class/3 Class>  │ Name                       │
│ <Name> | <Architecture> | <2 Class/3 Class>  │ Active: Yes / No           │
│ <Name> | <Architecture> | <2 Class/3 Class>  │ Architecture               │
│                                              │ Starting Weights           │
│                                              │ Classes and Class Names    │
│                                              │ Source Dataset             │
│                                              │ Creation Date              │
│                                              │ Package Location           │
│ [ Add Model ] [ Import Model ] [ Remove Model ]│ Training metadata/metrics │
│                                              │                            │
│                                              │ [ Set Active ]             │
│                                              │ [ Open in Model Test ]     │
│                                              │ [ Export ] [ Duplicate ]   │
│                                              │ [ Rename ]                 │
└──────────────────────────────────────────────┴────────────────────────────┘
```

## 14.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Inspect/manage a local Model Package and choose the global Active Model. |
| **Major regions** | Model list/master; selected-model detail; metadata/metrics; package location; action region; contextual fault banner. |
| **Dominant hierarchy** | Model list selection and explicit selected-vs-Active distinction. |
| **Operation-side panel** | Selected-model details and actions; may serve as the master-detail inspector. |
| **Primary action** | Add Model or Import Model when Empty; Set Active for a valid nonactive selected model. |
| **Secondary actions** | Add Model; Import Model; Remove Model; Export Model; Duplicate Model; Rename Model; Open in Model Test. |
| **Required artifact/hardware** | Valid v2 Model Package for package actions. No hardware. Package and registry locks apply. |
| **Output** | Model Registry changes and file copies/renames/removals as requested. |
| **Direct disabled reasons** | **No Active Model** → **Selected model is already Active** for Set Active; **Model is in use by <running operation>** for locked mutation/removal. An idle Model Test selection/reference is not an in-use consumer. File-action failures use direct package/path reasons. When no consumer is running, being the currently Active Model does not disable Remove Model. |
| **Fault banner** | Below workspace heading; spans master-detail region for validation, parse, copy, rename, remove, registry, or permission failures. |
| **Applicable presentations** | Empty, Ready, Unavailable for a locked action, Failed. |
| **Next likely action** | Open selected model in Model Test or navigate to Live after explicitly setting Active. |

## 14.4 Selected versus Active Model

These states MUST be independent:

- **Selected Model** is the row currently displayed in the detail pane. It uses selection fill/indicator and selected state exposure.
- **Active Model** is the one global Model Registry selection. Its persistent factual indication appears in the global header and selected-model detail, not as an additional Library-row field.

Selecting a row must not activate it. Set Active must be explicit. The detail pane should show `Active: Yes` or `Active: No` in text.

## 14.5 Model list

Each row MUST show exactly:

- Model Name;
- Architecture (`MobileNetV3-Small` or `EfficientNet-B0`);
- Class Type, rendered exactly as `2 Class` or `3 Class`.

Rows do not show descriptions, class-name lists, classes prose, creation date, or an additional Active field/marker. Selection remains structural; the global header and selected-model detail preserve the factual Active distinction. Technical action locks and failures are presented in the selected detail/action reason or fault banner rather than as extra row prose.

The list MAY support text filtering or stable column sorting as a presentation aid if it does not create a managed scientific status. Empty and filter-empty states must be distinct.

## 14.6 Selected-model detail

The detail pane includes factual package information:

- Model Name and ID;
- Active state;
- Architecture and approved Starting Weights;
- Class IDs and stored Class Name snapshot;
- source Dataset identity when recorded;
- creation date;
- package location;
- training configuration/provenance and factual metrics;
- file/checksum facts where useful for technical inspection.

Metrics use neutral presentation. No status badge derives from metric values.

## 14.7 Package actions

| Action | Design and behavior |
|---|---|
| **Set Active** | Primary when a valid nonactive package is selected and replacement is not locked. Successful activation updates header immediately. |
| **Add Model** | Opens one popup requiring a nonblank unique Name, one supported Architecture (`MobileNetV3-Small` or `EfficientNet-B0`), and exactly one visible Starting Weights choice: `ImageNet` or `Pretrained`. Each is a distinct fixed bundled architecture-specific local checkpoint and is enabled only when its corresponding artifact is locally valid. The `Pretrained` Starting Weights choice is not itself a Library model; the two installer-owned ready packages are separate complete Model Packages under §2.1.1. Library owns this identity. The selector never downloads, substitutes, or uses a network fallback. No additional architecture, option, or package schema is added. |
| **Import Model** | Selects `metadata.json` inside one complete supported v2 Model Package and performs the existing technical and package-integrity checks. Raw weights, a bare ONNX file, an incomplete package, or any selection other than the package's `metadata.json` is not importable. Import does not perform conversion or assign scientific state. |
| **Remove Model** | Adjacent to Import Model. It retains the existing direct confirmation naming the Model Package and consequence. When no consumer is running, the selected package may be removed even when it is currently Active. After confirmation, move the OpenDSS-owned complete package folder and files to the Windows Recycle Bin, update the registry, and leave Active Model empty. Do not infer or activate a fallback model. A genuinely running consumer still blocks removal. Preserve package-integrity checks and never perform direct permanent deletion. An idle Model Test selection/reference does not make the package in use. |
| **Export Model** | Writes the complete package to a user-selected location. |
| **Duplicate Model** | Creates an independent package copy with a new Model ID and user-provided name/location. |
| **Rename Model** | Changes the user-facing Model Name without altering weights, Class IDs, or scientific metadata. |
| **Open in Model Test** | Opens Models > Model Test with the selected package preselected without changing Active Model. |

Import Model and Remove Model are adjacent in the Model-list action region. Other secondary package actions SHOULD be grouped below Set Active/Open in Model Test. This placement and terminology do not add or alter destructive behavior beyond the existing confirmed package-removal contract.

The repository ships a documented standalone PyTorch conversion script as a development utility outside the OpenDSS application and Library UI. It produces a complete valid Model Package that is then subject to the same `metadata.json` import and integrity checks. It supports only MobileNetV3-Small and EfficientNet-B0. Library does not invoke the script, expose conversion options, accept raw weights or bare ONNX files, or bypass the v2 package contract.

Library owns model identity and creation. Train consumes the Library-defined Name, Architecture, and Starting Weights read-only. Retraining an already-trained package requires Add Model to define a different nonblank unique Name and always produces a new Library entry/package; the source identity and artifacts remain intact, read-only, and protected. Train publishes the newly named package atomically only after successful completion.

## 14.8 Lock treatment

- A package consumed by an actually running Model Test, Live, or Sequence Test operation cannot be renamed, removed, replaced, or otherwise mutated. Idle selection/reference is not use.
- When no consumer is running, the current Active Model may be removed after the existing confirmation. Successful removal moves its package/files to the Windows Recycle Bin, clears the Active Model registry selection, and leaves Active Model empty; no fallback model is inferred.
- When an operation uses the current Active Model, Set Active cannot replace it until the operation ends.
- Unrelated packages remain manageable.
- The locked row/detail displays the owning operation in text, for example `In use by Live Sorting`.
- Lock styling must not resemble invalid, failed, or scientifically rejected state.
- Import Model and Remove Model preserve genuinely running in-use, Model Registry, and package-integrity safety; neither action provides a bypass. An idle Model Test selection/reference is not a running in-use lock. Removing an idle Active Model updates the registry to empty rather than selecting a fallback.

## 14.9 Keyboard, resizing, and mock states

- Up/Down moves row focus; selection updates on platform-consistent activation without triggering Set Active.
- `Enter` MAY open details or perform the documented row open behavior, but never implicitly activate.
- Delete key must not remove a package without the named confirmation.
- Master/detail splitter is resizable. At minimum width, the list remains visible and details may open as a full-height inspector; switching presentation does not change selection.
- Required mocks: empty library; Add Model popup empty/duplicate Name; MobileNetV3-Small and EfficientNet-B0 identities; one Active model; multiple models; selected nonactive model; idle selected Active model with Remove Model available; Active Model empty after confirmed removal; two-class and three-class rows using exact `2 Class`/`3 Class` text; package selected by idle Model Test; package in use by a running Model Test; Active Model in use by Live; unreadable package; import failure; rename failure; remove confirmation; retraining source protected while a distinct new Name is created.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.6, §8, §13, §16, and D-010; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.9, and §6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §11, §16, §17, and §18; *OpenDSS Detailed User Workflow Specification* §17 as amended to v2-only import and no scientific states; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §8 master-detail evidence as adapted.

---
# 15. Sort > Live design

## 15.1 Design purpose and user goal

Live is one stateful workspace that combines pre-run configuration and active physical sorting. Its design purpose is to keep the live Camera view continuous while the right-side panel transitions from configuration to operation monitoring and then to factual Run outcome. There is no separate Sort Setup workspace.

The main user goal is to configure and conduct one Live Sorting Run, monitor Predicted Class, Decision, and Observed Route as separate facts, pause or stop the operation, and move directly to persisted Run results.

## 15.2 State overview

```text
Unavailable ⇄ Ready → Starting → Running ⇄ Paused
                                  │          │
                                  └── Stop ──┘
                                      ↓
                                   Stopping
                                 ↙     ↓      ↘
                         Completed  Interrupted  Failed
```

Live MUST use the same navigation destination for every presentation. The live Camera preview remains the dominant left-side region in pre-run, Starting, Running, and Paused states when the Camera remains functional.

## 15.3 Live design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Configure, start, monitor, pause/resume, stop, and inspect the immediate result of one physical sorting Run. |
| **Major regions** | Live Camera preview; operation-side panel; Setup Profile controls in pre-run; state/counter monitor during operation; completion/fault content after finalization; contextual fault banner. |
| **Dominant hierarchy** | Pre-run: Camera preview and Start readiness. Running: Camera preview, Run status, Decision/Observed Route counters, Pause/Stop. Post-run: outcome and preserved Run actions. |
| **Required hardware/artifact** | Camera Streaming; Decision Boundary; writable Run location; global operation slot. Class-Based Sorting also requires a loadable Active Model and Hit Class. DAQ Ready is required only while DAQ Output is ON. |
| **Output** | Run folder with `run_summary.json`, `events.csv`, Droplet Crops, and optional full Image Sequence. |
| **Hardware behavior** | Panel available in idle pre-run. On Start acceptance it closes and locks completely until the Run ends. Live owns Camera and DAQ. |
| **Fault banner** | Below workspace heading and above preview/panel. In post-run Interrupted/Failed presentation it may become the dominant outcome block. |
| **Applicable presentations** | Unavailable, Ready, Starting, Running, Paused, Stopping, Completed, Interrupted, Failed. |
| **Next likely action** | Open Run Summary in Results, Open Run Folder, or Start New Run. |

## 15.4 Pre-run layout

```text
Sort > Live

┌──────────────────────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW                          │ LIVE RUN CONFIGURATION       │
│                                              │                              │
│                                              │ Setup Profile                │
│                                              │ [ Open ] [ Save ] [ Save As ]│
│                                              │                              │
│                                              │ Run Name                     │
│                                              │ Experiment Type              │
│                                              │ Notes                        │
│                                              │ Duration                     │
│                                              │ Save Location                │
│                                              │                              │
│                                              │ Trigger Every Droplet                 │
│                                              │ ( ) Class-Based Sorting      │
│                                              │ ( ) Trigger Every Droplet    │
│                                              │                              │
│                                              │ Active Model: <name/none>   │
│                                              │ Hit Class [ class ▼ ]       │
│                                              │ Decision Boundary            │
│                                              │ [ Set Decision Boundary ]    │
│                                              │ ( ) Top is Hit               │
│                                              │ ( ) Bottom is Hit            │
│                                              │ Hit side: <Top/Bottom>       │
│                                              │ Waste: <opposite>            │
│                                              │                              │
│                                              │ [ ] Record Full Image        │
│                                              │     Sequence                 │
│                                              │                              │
│                                              │ [ Send Test Pulse ]          │
│                                              │ [ Start Sorting ]            │
└──────────────────────────────────────────────┴──────────────────────────────┘
```

Camera, DAQ, and Detector Configuration settings are never duplicated in this panel. A concise read-only configuration summary may link to the bottom Configuration panel.

## 15.5 Pre-run fields and visual grouping

Fields are grouped in this order:

1. **Setup Profile** — loaded filename/status and Open, Save, Save As.
2. **Run information** — Run Name, Experiment Type, Notes, optional Duration, Save Location.
3. **Trigger and routing** — Trigger Every Droplet, Active Model where relevant, Hit Class where relevant, Decision Boundary placement and explicit Hit/Waste mapping.
4. **Output retention** — Record Full Image Sequence.
5. **Hardware action** — Send Test Pulse.
6. **Primary action** — Start Sorting and one direct disabled reason.

Run Name, Experiment Type, Notes, Duration, and Save Location are user selections, not technical tuning settings. Blank Run Name resolves to a timestamp. Blank Duration means continue until Stop and must be represented as empty/optional rather than zero.

## 15.6 Trigger Every Droplet behavior

### Class-Based Sorting

- Active Model is required and shown read-only from the global Model Registry.
- Hit Class is required and lists Class ID plus Class Name from the Active Model.
- Predicted Class is the class with the largest Class Score.
- Decision is Hit only when Predicted Class equals Hit Class; all other predictions produce Decision Waste.
- For an accepted droplet, enabled DAQ output follows Decision: Decision Hit requests physical output and Decision Waste does not.
- No Class Score or confidence threshold is editable or used to alter routing.

### Trigger Every Droplet

- Active Model is optional.
- Hit Class is hidden or disabled with a clear `Not used for Trigger Every Droplet` explanation.
- Every accepted droplet produces Decision Hit and therefore requests physical output when DAQ output is enabled.
- If a model is present, Predicted Class and Class Scores may still be logged and displayed; they do not control Decision.
- If no model is present, model-dependent fields remain absent or explicitly empty rather than displaying fabricated values.

Trigger Every Droplet choices are first-class peers and remain visible in pre-run. No default should silently imply a scientific choice unless the authoritative application state already defines one.

## 15.7 Decision Boundary

The visible concept and control label is `Decision Boundary`. It is the horizontal observer/comparison line used to calculate Observed Route. It is not a Decision or DAQ arbitration line.

Placement is explicit:

1. `Set Decision Boundary` arms one placement click in the owning workspace's currently visible full-size frame.
2. Ordinary frame clicks while placement is not armed MUST NOT create, move, or clear the boundary.
3. The armed click maps its X and Y position through the displayed-frame transform to exact source-image coordinates.
4. The observer Decision Boundary segment begins at that clicked source-image X/Y point and extends horizontally to the **right edge** of the frame. It MUST NOT extend leftward and MUST NOT span the full frame. The overlay is shown only in the owning viewer.
5. After the successful click, placement disarms and focus returns to the Decision Boundary controls.
6. `Reset` clears the owning workspace's boundary.

The clicked X and Y coordinates are workspace-local authoritative observer state. Live and Sequence Test own independent Decision Boundaries and MUST NOT read, reuse, overwrite, synchronize, or display each other's boundary.

The coordinate MUST NOT be written to Setup Profiles, Run files, `events.csv`, `run_summary.json`, Results, logs, or any other exported artifact. No coordinate history, timestamp, event association, or cross-workspace projection may be inferred.

Droplets flow left to right and EventDetector tracks each droplet through the frame. Observed Route timing is:

- While a tracked droplet remains in the frame, including while it is left of the clicked Decision Boundary X, the observer calculation remains pending.
- One missed frame does not end the authoritative track. The same track remains pending through that miss, and a reappearance updates its latest tracked source-image Y. Observed Route finalizes only when the qualified detector reports the track lifecycle ended.
- The clicked X defines only the start of the visible right-edge segment. It is not a Decision or DAQ threshold.
- Observed Route is calculated when the track ends or the droplet disappears. The final tracked source-image Y is compared with the Decision Boundary Y, and `Top is Hit` / `Bottom is Hit` supplies the observed Hit/Waste mapping.
- The same final-Y comparison applies even in the unexpected case where the track ends or disappears before ever reaching the clicked X; clicked X alone never makes the outcome Unresolved.
- When final Y is exactly equal to the Decision Boundary Y, Observed Route is `Unresolved`. Equality MUST NOT be forced to observed Hit or Waste.

This entire boundary/final-Y calculation is observer-only. It never changes Predicted Class or Decision and never requests, suppresses, or otherwise controls DAQ output. Trigger Every Droplet OFF makes the model Decision control DAQ; Trigger Every Droplet ON makes every accepted droplet Decision Hit. Observed Route is persisted only for factual comparison with Decision and is never a DAQ gate.

The product default observer mapping is `Bottom is Hit` in both Live and Sequence Test. The user may choose either mapping:

```text
Top is Hit
Bottom is Hit
```

The panel immediately displays the resulting mapping:

```text
Top is Hit:    Hit = above boundary   Waste = below boundary
Bottom is Hit: Hit = below boundary   Waste = above boundary
```

This mapping should use a small coordinate diagram plus text. It must not use the ambiguous term `Hit Channel`. Decision Boundary remains separate from DAQ Output Channel.

Start is unavailable until the owning workspace has a Decision Boundary. The default or user-selected observer side is already defined. The direct disabled reason is **No Decision Boundary set**.

The authoritative `UAT-TRAJECTORY-001` fixture root is `C:\Users\goals\OneDrive\Documents\OpenDSS Renewal\Droplet trajectory test`. Its frames are `1200 × 360`, so the fixture boundary is exactly source-image `Y = height / 2 = 180`, with `Bottom is Hit`. Expected Observed Route is derived only from the final tracked source-image Y relative to that boundary; directory names and legacy behavior remain evidence, not product authority. This fixture characterizes observer-only final-Y behavior, including one-miss retention and lifecycle-end finalization. It MUST NOT change model Decision or request or suppress DAQ output.

Decision Boundary remains editable during an active Live or Sequence Test Run. A successfully placed replacement boundary becomes the observer Hit/Waste line immediately for subsequent Observed Route calculations; it does not rewrite prior events or create persisted boundary provenance. `Reset` clears the boundary; a new Start remains unavailable until it is set again.

The selected Top-is-Hit/Bottom-is-Hit mapping may follow the applicable profile and Run configuration contracts, but the boundary coordinate is explicitly excluded from all persistence and provenance.

During active sorting, Trigger Every Droplet, Hit Class, Physical DAQ Output, voltage, frequency, duration, delay, Decision Boundary Set/Reset, and Top-is-Hit/Bottom-is-Hit remain editable. A successfully committed change applies only to subsequent droplets and never rewrites a prior event. Active Model remains read-only, and Send Test Pulse is disabled throughout active sorting.

The existing Run format remains unchanged. There is no configuration history, schema-version increase, new configuration-history field, or new event-to-configuration ID. Existing event configuration linkage remains in its current `initial` form. When the Run ends, its existing single configuration snapshot is finalized with only the last successfully active Trigger/DAQ values and Top-is-Hit/Bottom-is-Hit mapping. Intermediate values and change timing are not persisted. Decision Boundary X/Y coordinates remain workspace-local presentation state and are never written to the final snapshot or any Run, profile, event, log, result, or export.

## 15.8 Active Model presentation

- The Active Model row includes the model name, two-class/three-class fact, and a link to Library or model detail only when doing so does not disrupt pre-run state.
- `No Active Model` is neutral in Trigger Every Droplet and Unavailable for Class-Based Sorting.
- Loading a valid Setup Profile model reference may update the global Active Model in pre-run according to the approved profile behavior.
- Once Start is accepted, the model identity is part of the immutable Run configuration snapshot. Later model changes elsewhere do not alter the active Run.

## 15.9 Send Test Pulse

Send Test Pulse is a secondary hardware action:

- it requires DAQ Ready and valid applied DAQ settings;
- it is disabled when another operation owns DAQ;
- it issues one pulse and creates no Run or Droplet Log event;
- it must not appear as arming, readiness approval, or a safety-rated Emergency Stop function;
- feedback is concise and local, for example `Test pulse sent` or a direct DAQ failure reason.

Because it causes physical output, the action should use text plus an output/pulse icon and remain visually subordinate to Start Sorting. A tooltip must not be the sole warning that it produces physical output.

## 15.10 Start enablement and disabled reasons

Start Sorting is enabled only when:

- no conflicting long-running operation is active;
- Camera is Streaming;
- DAQ is Ready;
- Trigger Every Droplet is selected;
- a Decision Boundary is set;
- Run location is writable;
- for Class-Based Sorting, a loadable Active Model and Hit Class are selected.

The first applicable direct disabled reason is:

1. **Another operation is active**;
2. **Camera unavailable**;
3. **DAQ unavailable**;
4. **No Trigger Every Droplet selected**;
5. **No active model** for Class-Based Sorting;
6. **No Hit Class selected** for Class-Based Sorting;
7. **No Decision Boundary set**;
8. **Output folder is not writable**.

A Camera preview that is merely Connected but not Streaming is treated as the applicable Camera prerequisite failure and must not show Start as Ready.

## 15.11 Starting transition

On accepted Start:

```text
PRE-RUN READY
    │ Start Sorting accepted
    ▼
STARTING
    ├── create Run ID and folder
    ├── write initial run_summary.json
    ├── open recoverable Droplet Log
    ├── snapshot effective user selections and fixed configuration,
    │   excluding the workspace-local Decision Boundary X and Y point
    ├── lock Camera, DAQ, selected model, Run output, and global slot
    ├── lock Camera and DAQ sections in Configuration
    └── replace configuration panel with Live monitor
    ▼
RUNNING
```

Visual requirements:

- Start Sorting becomes a busy `Starting…` action immediately.
- Non-boundary pre-run inputs become read-only before the panel transitions; they are not briefly left editable.
- Decision Boundary placement, mapping, and Reset remain available after Start, including during Running and Paused.
- Camera and DAQ sections in Configuration become read-only. Detector Configuration remains available, and focus returns to the Live status heading after Start acceptance.
- The operation panel may show named initialization stages and an indeterminate progress indicator.
- Stop becomes available as soon as cancellation can be handled safely.
- A Start failure before acceptance leaves Live in Ready/Unavailable with the direct reason and must not create a duplicate or misleading Run.

## 15.12 Running layout and metric hierarchy

```text
┌──────────────────────────────────────────────┬──────────────────────────────┐
│ LIVE CAMERA PREVIEW                          │ LIVE SORTING                 │
│                                              │ Status: Running              │
│                                              │ Elapsed: <time>              │
│                                              │ Trigger Every Droplet: <value>        │
│                                              │ Active Model: <value>        │
│                                              │ Hit Class: <value>           │
│                                              │ Decision Boundary: <value>   │
│                                              │                              │
│                                              │ Total Droplets               │
│                                              │ <large metric>               │
│                                              │ Rejected <count>              │
│                                              │                              │
│                                              │ Predicted Class counts       │
│                                              │ Decision Hit / Waste         │
│                                              │ Observed Hit / Waste /       │
│                                              │ Unresolved                   │
│                                              │                              │
│                                              │ Inference Time               │
│                                              │ Camera FPS                   │
│                                              │                              │
│                                              │ [ Pause ]  [ Stop ]          │
└──────────────────────────────────────────────┴──────────────────────────────┘
```

The metric hierarchy is:

1. Run status and elapsed time;
2. Total Droplets;
3. Rejected;
4. Decision Hit and Decision Waste;
5. Observed Hit, Waste, and Unresolved;
6. Predicted Class counts when a model exists;
7. Inference Time and camera FPS;
8. immutable routing/model context.

Rejected, Predicted Class, Decision, and Observed Route must use separate titled groups. Observed Route must always provide the Unresolved value. Counts use tabular figures and must update from finalized event state, not by repeatedly rereading the CSV.

When no model is used, Predicted Class and Inference Time groups are absent or display `Not recorded — no model`, while Decision and Observed Route remain present.

## 15.13 Running interaction behavior

- Live real-time sorting inference uses the qualified C++ ONNX Runtime CPU path. GPU status is not presented as a Live prerequisite.
- Decision Boundary placement and mapping remain editable after Start. `Set Decision Boundary` arms exactly one replacement click; ordinary frame clicks remain inert.
- The replacement segment begins at the new clicked source-image X/Y point and extends horizontally to the right edge only. It applies immediately to subsequent Observed Route calculations and never rewrites a prior event.
- No timestamped boundary history or boundary-version association is created, and no clicked X or Y value is written to a Run, event, Result, log, profile, or exported artifact.
- Every Live or Sequence Test detection writes one Droplet Log row whose integer `rejected` field is exactly `0` or `1`. A normal accepted row writes `rejected=0`.
- An undersized detection writes `rejected=1` and produces no crop image, inference, Decision, Observed Route, or DAQ output, including when Trigger Every Droplet is ON. No `rejection_reason` field or column exists.
- Rejected is a separate count. Rejected rows are excluded from Total Droplets and from all Predicted Class, Decision, and Observed Route counts.
- Every non-rejected finalized event saves one Droplet Crop. Record Full Image Sequence controls only full-frame retention.
- Optional nonpersistent detection/trajectory overlays MAY appear on the live preview when they add no controls, do not alter source images, and do not obscure the factual feed.
- The Configuration action remains available during Live. Camera and DAQ sections are read-only with **Camera and DAQ settings are locked while sorting is active**; Detector Configuration remains editable.
- Navigation away is permitted when nonconflicting, but Pause/Resume/Stop remain available only in the owning Live workspace. The header continues to show Sorting.
- Start actions for all other long-running operations show **Another operation is active**.

## 15.14 Paused presentation

```text
LIVE SORTING
Status              Paused
Active Run time     <frozen>
Counters            <stable>

Live camera preview continues.
No inference, new DAQ output, or new event finalization occurs.
Camera and DAQ settings remain locked. Detector Configuration remains editable.

[ Resume ]  [ Stop ]
```

Paused uses the same Run identity and configuration snapshot. It must not visually resemble Completed or Ready. The global header shows `Paused`, Camera remains `Streaming`, and DAQ remains `Active` because the paused Run still owns it.

Pause must flush applicable persistence queues. Resume continues the same Run and returns to Running without reopening configuration.

## 15.15 Stopping and finalization

Stopping communicates:

- new inference has stopped;
- new DAQ commands have stopped;
- no new events are accepted;
- queued Droplet Crops, event rows, and optional sequence frames are being flushed;
- `events.csv` and `run_summary.json` are being finalized as far as technically possible.

Pause/Resume and Start are disabled. Stop is idempotent and changes to a noninteractive `Stopping…` state after acceptance.

## 15.16 Completed, interrupted, and failed presentations

Completed or clean user Stop:

```text
Live Run completed / stopped
Status: <Completed or Stopped>
Stop reason: <duration | user | other factual reason>
Total Droplets: <count>
Location: <path>

[ Open Run Summary ]  [ Open Run Folder ]
[ Start New Run ]
```

A clean user Stop uses this post-operation presentation even when persisted status is `Stopped`.

Interrupted/Failed:

```text
┌─────────────────────────────────────────────────────────────────────────┐
│ Live Sorting interrupted                                                │
│ Error                                               │
│ <whether recoverable Run data, Droplet Log, crops, or sequence remain> │
│                                                                         │
│ [ Open Run Summary ] or [ Open Run Folder ]    [ Start New Run ]       │
└─────────────────────────────────────────────────────────────────────────┘
```

Hardware faults must stop new impossible or unsafe output before the UI message. The banner must not claim a Run Summary is available unless it is readable.

Start New Run returns to pre-run with previous values available for review. It does not duplicate or automatically start the prior configuration.

## 15.17 Keyboard and focus behavior

- Tab order in pre-run follows profile → Run information → Trigger Every Droplet → conditional model/Hit Class → Decision Boundary controls → full-sequence option → Send Test Pulse → Start Sorting.
- Selecting Trigger Every Droplet must preserve logical focus as conditional fields appear/disappear.
- During Running, focus order starts at status, then metrics, Pause/Resume, and Stop.
- No unmodified global key may start or stop physical sorting.
- An optional displayed two-key accelerator for Pause/Resume MAY be provided, but it must not fire while typing and must be disabled outside the owning Live state.
- Stop must always be reachable by Tab and an accessible name. Escape does not Stop; it closes overlays only.
- On fault, focus moves to the banner heading only once, then proceeds to direct actions. Repeated hardware-state updates must not repeatedly steal focus.

## 15.18 Maximized-window layout

- Live preview receives the flexible majority of width.
- The operation panel SHOULD remain 380–440 logical px in the maximized validation layout.
- During Running, less-critical provenance may collapse into `Run configuration` disclosure, but status, elapsed time, Total Droplets, Decision counts, Observed Route counts, Pause/Resume, and Stop remain visible.
- Counter groups may change from multi-column to stacked layout when available space requires it, without changing labels or grouping.
- The Camera preview must retain a useful aspect-preserving area and must not be hidden by the operation panel.

## 15.19 Minimum mock-data states

1. Camera unavailable and DAQ unavailable.
2. Camera Streaming, DAQ Ready, no Active Model, Class-Based selected.
3. Trigger Every Droplet with no model.
4. Two-class Active Model and Hit Class selection.
5. Three-class Active Model and Hit Class selection.
6. Both Decision Boundary mappings, set and Reset, and right-edge-only segment geometry.
7. Loaded Setup Profile, unsaved changes, missing model reference, unapplied hardware values.
8. Send Test Pulse success and failure.
9. Ready with full-sequence option off/on.
10. Starting with named initialization stage.
11. Running with model and complete counters.
12. Running without model.
13. Large counters and long model/Run names.
14. Paused.
15. Stopping.
16. Completed by Duration.
17. Stopped by user.
18. Interrupted by Camera fault with preserved data.
19. Interrupted/Failed by DAQ or write fault.
20. Run with and without full Image Sequence.
21. Configuration available with Camera/DAQ locked and Detector Configuration editable.
22. Another-operation-active blocker.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.7, §§8–14 and D-002, D-005, D-007, D-008, D-009, D-016, D-019; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 3.2, 4.10–4.13, §§5–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §12, §16, §§17–19; *OpenDSS Detailed User Workflow Specification* §§20–21 and §§28–34 as amended to one Live workspace, fixed processing, ordinary-file Profiles, and Observed Route = Unresolved; *OpenDSS Product Design Specification*, Draft v0.1 §§5–7 and §§9–10 as adapted visual, component, and accessibility evidence.

---

# 16. Sort > Sequence Test design

## 16.1 Design purpose and user goal

Sequence Test reprocesses a recorded Image Sequence through fixed droplet detection and crop processing, optional or required model inference according to Trigger Every Droplet, routing logic, visual trajectory tracking, optional physical DAQ output, and Run persistence. It belongs under Sort and never requires a Camera.

The main user goal is to select a sequence and routing configuration, explicitly confirm whether physical DAQ output is enabled, run the test, monitor progress and event counters, stop if needed, and open the resulting Run.

## 16.2 Layout

```text
Sort > Sequence Test

┌──────────────────────────────────────────────┬──────────────────────────────┐
│ SOURCE AND PROCESSING SUMMARY                │ SEQUENCE TEST SETUP          │
│ Sequence: <name> [ Select Sequence ]         │ Trigger Every Droplet                 │
│ Frames: <count>                              │ ( ) Class-Based Sorting      │
│                                              │ ( ) Trigger Every Droplet    │
│ Active Model: <name/none>                  │                              │
│                                              │ Hit Class [ class ▼ ]        │
│ Fixed qualified processing                   │ Own Decision Boundary        │
│                                              │ Point X [value] Y [value]    │
│                                              │ ( ) Top is Hit               │
│                                              │ ( ) Bottom is Hit            │
│                                              │                              │
│                                              │ [ ] Physical DAQ Output      │
│                                              │ Save Location [_______] […]  │
│                                              │                              │
│                                              │ [ Start Sequence Test ]      │
└──────────────────────────────────────────────┴──────────────────────────────┘
```

A new Sequence Test setup initializes Physical DAQ Output as unchecked. This default must be visibly explicit and must not be hidden in a panel.

## 16.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Process a recorded Image Sequence through sorting logic and optionally issue physical DAQ output. |
| **Major regions** | Source/processing summary; setup/operation panel; progress and counters; completion/fault banner. |
| **Dominant hierarchy** | Selected sequence, explicit Physical DAQ Output state, routing selections, Start readiness. During Running: progress, physical-output state, event counters, Stop. |
| **Operation-side panel** | Trigger Every Droplet; read-only Active Model; Hit Class when applicable; Decision Boundary controls; Physical DAQ Output; Save Location; Start/Stop; status. |
| **Primary action** | Start Sequence Test in Ready; Stop Sequence Test in Running; Open Run Summary in Completed. |
| **Secondary actions** | Select Sequence; Set Active in Library; toggle Physical DAQ Output; Open Run Folder; Start Another Sequence Test. |
| **Required artifact/hardware** | Readable v2 Image Sequence; writable Run location; global operation slot. Class-Based requires compatible Model and Hit Class. DAQ Ready required only when Physical DAQ Output is enabled. No Camera. |
| **Output** | Run folder with `run_summary.json`, `events.csv`, Droplet Crops, and source-sequence reference. |
| **Direct disabled reasons** | **Another operation is active** → **No sequence selected** → **Unsupported OpenDSS v2 sequence** → **No Trigger Every Droplet selected** → **No Active Model** for Class-Based → **No Hit Class selected** for Class-Based → **No Decision Boundary set** → **DAQ unavailable** when physical output enabled → **Output folder is not writable**. |
| **Fault banner** | Below workspace heading and above source/setup region; post-operation banner may lead directly to Results/folder. |
| **Applicable presentations** | Empty, Unavailable, Ready, Starting, Running, Stopping, Completed, Interrupted, Failed. No Pause. |
| **Next likely action** | Open Run Summary in Results. |

## 16.4 Trigger Every Droplet and model selection

- Class-Based Sorting requires a selected readable two-class or three-class Model Package and Hit Class.
- Trigger Every Droplet may run with no model. When a model is selected, classification is still logged without controlling Decision.
- Model selection is local to Sequence Test and MUST NOT change Active Model.
- The read-only Active Model context displays Class IDs/Class Names and compatibility facts as needed.
- No Class Score threshold or manual compute-device control is shown.
- Sequence Test real-time sorting inference uses the qualified C++ ONNX Runtime path and defaults to CPU.

Sequence Test owns and edits its own workspace-local observer Decision Boundary. It MUST NOT read, reuse, overwrite, display, or synchronize Live state. `Set Decision Boundary` arms exactly one placement click; ordinary frame clicks are inert. The click maps through the current presentation transform to an exact source-image X/Y point. The horizontal observer segment begins at that point and extends to the right edge of the source frame only; it never extends leftward or across the full frame. `Bottom is Hit` is the default; the user may select `Top is Hit`. Reset clears the boundary. Start is unavailable until the boundary is set. The clicked point MUST NOT leave the Sequence Test workspace or be persisted, logged, exported, or associated with an event or history.

When Physical DAQ Output is enabled, accepted-droplet output follows Decision exactly as defined in §15.6. The observer Decision Boundary and persisted Observed Route never control DAQ output.

## 16.5 Physical DAQ Output

The checked state means physical output is requested for the test. The control must include:

- explicit checkbox label `Physical DAQ Output`;
- secondary status line `DAQ Ready`, `DAQ unavailable`, or `Off — no physical output`;
- a direct disabled reason at Start when checked and DAQ is unavailable;
- factual explanation that disabling output permits processing without DAQ hardware.

When checked, Sequence Test owns DAQ from Start acceptance through finalization. The DAQ section of the bottom Configuration panel locks; Camera and Detector Configuration remain independently available. When unchecked, Sequence Test owns no hardware and the panel remains available, although the global long-running-operation slot is still occupied.

The checkbox is not a software arming state and does not create an Emergency Stop claim.

## 16.6 Camera independence and fixed processing

- No Camera preview region appears and Camera status is never a prerequisite.
- The source area may show the first frame or a representative static preview as a read-only sequence fact, but must not imply live acquisition.
- Fixed detector, Droplet Crop, routing-algorithm, and internal timing configuration is described factually and not editable. Sequence Test's own Decision Boundary is the explicit operational exception and remains editable in setup.
- Processing controls from Sequence Viewer is not shown and has no effect. Sequence Test processes at the configured Processing FPS.
- Preview transport is presentation-only and never throttles, pauses, reorders, or changes scientific frame processing, event finalization, Decision, Observed Route, or physical DAQ timing.

## 16.7 Running presentation

```text
┌──────────────────────────────────────────────┬──────────────────────────────┐
│ SEQUENCE PROCESSING                          │ SEQUENCE TEST STATUS         │
│ Frames processed: <n> of <total>             │ Status: Running              │
│ Progress: <determinate bar>                  │ Trigger Every Droplet: <value>        │
│ Total Droplets / Rejected: <counts>          │ Physical DAQ: On / Off       │
│                                              │ Model: <name/none>           │
│ Predicted Class counts when model exists     │ Hit Class: <value>           │
│ Decision Hit / Waste                         │ Decision Boundary: <value>   │
│ Observed Hit / Waste / Unresolved            │                              │
│                                              │ [ Stop Sequence Test ]       │
└──────────────────────────────────────────────┴──────────────────────────────┘
```

Progress is based on sequence frames processed and may additionally show events finalized. Rejected, Predicted Class, Decision, and Observed Route remain separate. Rejected is excluded from Total Droplets; Observed Route includes Unresolved.

The Running viewer retains the last successfully rendered frame until a newer processed frame is renderable; it never clears or becomes blank merely because processing is ahead of rendering. Intermediate preview frames MAY be coalesced or dropped. Whenever the viewer becomes ready, the newest processed frame available at that moment is published immediately so visible playback stays current with processing rather than replaying an accumulating backlog. Trigger execution and counters may advance between rendered frames, but a long interval of blank or stale presentation while multiple frames are processed and triggers fire is prohibited. Completion publishes the final processed frame without replaying dropped preview frames.

## 16.8 Stop, completion, and failure

- Stop halts new processing and physical output, then enters Stopping while Run files finalize.
- Sequence Test has no Pause state.
- Completion or clean Stop provides Open Run Summary, Open Run Folder, and Start Another Sequence Test.
- Every completed or stopped Sequence Test Run appears under Results > Runs.
- Interrupted/Failed banners state whether recoverable Run data exists and whether physical output had been active.
- Source Image Sequence is read-only and never modified.

## 16.9 Keyboard, resizing, and mock states

- Tab order: Sequence → Model → Trigger Every Droplet → conditional Hit Class → Decision Boundary controls → Physical DAQ Output → Save Location → Start.
- Toggling Physical DAQ Output updates Start reason and panel lock projection without moving focus unexpectedly.
- During Running, focus proceeds through status/progress to Stop.
- At minimum width, source summary stacks above setup; progress remains visible and Stop remains pinned in the operation region.
- Required mocks: no sequence; unsupported sequence; Class-Based/no model; Trigger Every Droplet/no model; two- and three-class models; DAQ enabled/Ready; DAQ enabled/unavailable; DAQ disabled/no hardware; both Hit directions; Starting; Running; Stopping; Completed; Interrupted; Failed; model package locked; no Camera connected while Ready.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.8, §§8, 10–14 and D-004, D-005, D-006, D-008; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.14, §§5–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §13, §16, §§17–19; *OpenDSS Detailed User Workflow Specification* §19 and persistence/Run requirements as amended for placement under Sort and Observed Route = Unresolved; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–10 as adapted visual and accessibility evidence.

---

# 17. Results > Runs design

## 17.1 Scope and design purpose

Results contains only Live Sorting and Sequence Test Runs. It does not contain Training history, Model Test history, Image Sequence capture history, Droplet Dataset Capture history, or separate Reports.

The workspace has two presentations:

1. a Runs list for discovery and selection;
2. a selected Run view for factual summary, provenance, Notes, and direct file access.

No first-class charts or integrated event-by-event Run browser are added.

## 17.2 Runs list

```text
Results > Runs

┌────────────────────────────────────────────────────────────────────────────┐
│ Run Name | Operation | Started | Duration | Status | Model | Total        │
│----------------------------------------------------------------------------│
│ <row>                                                                      │
│ <row>                                                                      │
│ <row>                                                                      │
└────────────────────────────────────────────────────────────────────────────┘

[ Load selected Run ]  [ Remove Run ]
```

### List design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Find and open one persisted Live Sorting or Sequence Test Run. |
| **Major regions** | Workspace heading; Runs table; optional simple filter/search if implemented; action region; fault banner. |
| **Dominant hierarchy** | Factual list columns and current selection. |
| **Primary action** | Load selected Run. |
| **Secondary actions** | Select another Run; Remove Run; open storage location only where a row remains identifiable but summary is unreadable. |
| **Required artifact/hardware** | Discoverable Run folders. No hardware. |
| **Output** | None. |
| **Direct disabled reasons** | **No Runs found** in Empty; **No Run selected** before selection; direct unreadable Run reason. |
| **Applicable presentations** | Empty, Ready, Failed. |

Required list columns:

- Run Name;
- operation type (`Live Sorting` or `Sequence Test`);
- start timestamp;
- duration;
- persisted status (`Completed`, `Stopped`, `Interrupted`, `Failed`);
- model name when present;
- Total Droplets.

Status values use factual semantic badges. A Failed Run is not removed from the list. A Run without a model displays `—` or `No model`, not `Unknown model`.

Rows must remain identifiable when optional metadata is missing. An unreadable entry may retain path/name facts and expose a direct file reason.

Remove Run performs no action until the user explicitly confirms. After confirmation it moves the complete selected Run folder, including all contained files, to the Windows Recycle Bin. It does not directly or permanently delete the Run; cancelling the confirmation changes nothing.

Only the Run-list body scrolls. `Load selected Run` and `Remove Run` remain in a fixed bottom action region and MUST NOT move out of view when the list is scrolled.

## 17.3 Selected Run layout

```text
┌──────────────────────────────────────────────┬──────────────────────────────┐
│ RUN SUMMARY                                  │ FILES AND NOTES              │
│ Run Name / operation / status                │ [ Open Droplet Log ]         │
│ Experiment Type / timestamps / Duration      │ [ Open Run Folder ]          │
│ Stop reason / Save Location                  │ [ Open Droplet Crop ]        │
│                                              │ [ Open Saved Sequence ]      │
│ Model identity/checksum when present         │   when present               │
│ Trigger Every Droplet                                 │                              │
│ Hit Class when applicable                    │ Notes                        │
│ Decision Boundary mapping                    │ <read or edit area>          │
│ Physical DAQ state for Sequence Test         │ [ Edit Notes ]               │
│                                              │ [ Save Notes ] [ Cancel ]    │
│ Hardware/fixed processing provenance         │                              │
│                                              │                              │
│ Total Droplets                               │                              │
│ Rejected                                     │                              │
│ Predicted Class counts                       │                              │
│ Decision Hit / Waste                         │                              │
│ Observed Hit / Waste / Unresolved            │                              │
│                                              │                              │
│ DECISION VS. OBSERVED ROUTE                  │                              │
│            Observed Hit Waste Unresolved     │                              │
│ Decision Hit       n     n       n           │                              │
│ Decision Waste     n     n       n           │                              │
└──────────────────────────────────────────────┴──────────────────────────────┘
```

### Selected Run design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Review factual Run outcome, provenance, and files without changing historical events. |
| **Major regions** | Run identity/status; experiment and timing; model/routing snapshot; hardware/fixed-processing provenance; counts; Decision-versus-Observed Route matrix; files; Notes. |
| **Dominant hierarchy** | Status and identity, Total Droplets, separate Rejected/Predicted Class/Decision/Observed Route counts, matrix, direct file actions. |
| **Operation-side panel** | Files and Notes inspector. |
| **Primary action** | Open Droplet Log when available. |
| **Secondary actions** | Open Run Folder; Open Droplet Crop; Open Saved Sequence when present; Edit/Save/Cancel Notes. |
| **Required artifact/hardware** | Readable `run_summary.json` and related files as available. No hardware. |
| **Output** | Notes-only atomic update to `run_summary.json`. Historical event data remains immutable. |
| **Direct disabled reasons** | **Droplet Log unavailable**; **Run Summary cannot be read**; **No saved Image Sequence for this Run**; **Run is still active**; **Run Summary is not writable**. |
| **Fault banner** | Below selected Run heading; file-specific actions may also show inline reasons. |
| **Applicable presentations** | Ready, Unavailable for missing individual files, Failed for open/save errors. |
| **Next likely action** | Open Saved Sequence in Sequence Viewer when present. |

## 17.4 Run information and provenance

The selected Run MUST display, as available:

- status, start/end timestamps, requested Duration, elapsed duration, stop reason, and save location;
- operation type and Experiment Type;
- model name/ID/checksum, class count, and stored Class Name snapshot when a model was present;
- Trigger Every Droplet, Hit Class when applicable, Decision Boundary owner (`Live` or `Sequence Test`), and the selected Top-is-Hit/Bottom-is-Hit mapping; clicked Decision Boundary X and Y coordinates are never Run provenance and MUST NOT appear in Results;
- Physical DAQ Output state for Sequence Test;
- Camera, DAQ, and fixed qualified processing configuration/version needed for provenance;
- OpenDSS and schema versions where recorded.

Provenance may use grouped disclosures to reduce initial density, but it must remain directly inspectable and must not be hidden behind a separate Reports workspace.

## 17.5 Counts and matrix

Counts are grouped and labeled exactly:

- Total Droplets;
- Rejected;
- Predicted Class count for each recorded class;
- Decision Hit;
- Decision Waste;
- Observed Hit;
- Observed Waste;
- Unresolved.

Rejected is excluded from Total Droplets and from the Predicted Class, Decision, Observed Route, and Decision-versus-Observed Route matrix counts.

The matrix title is `Decision vs. Observed Route` or `Decision-versus-Observed Route`. It includes the Unresolved column:

| Decision | Observed Hit | Observed Waste | Unresolved |
|---|---:|---:|---:|
| Hit | count | count | count |
| Waste | count | count | count |

The interface MUST NOT label this matrix Actual Destination, Ground Truth Route, Routing Accuracy, or Predicted Hit vs. Actual Hit. It must not automatically interpret percentages as acceptable or unacceptable.

## 17.6 Files and Notes

- Open Droplet Log opens `events.csv` through the operating system or approved direct file action.
- Open Run Folder opens the ordinary Windows folder.
- Open Droplet Crop opens a standard file picker rooted in the Run's Droplet Crop folder, then opens the selected file. This is not an integrated event browser.
- Open Saved Sequence appears only when a full Image Sequence exists and opens Sequence Viewer with it preselected.
- When no sequence exists, the action is absent or disabled with **No saved Image Sequence for this Run**; the Run remains Ready.
- Notes enter an explicit edit state. Save Notes atomically updates only Notes in `run_summary.json`; Cancel discards the buffer.
- Historical event rows, counts, model/routing snapshot, and saved images remain immutable.

## 17.7 Keyboard, responsive behavior, and mock states

- Table headers and cells expose accessible names. Up/Down changes focused row; selection does not load or replace the center Run.
- The right-side Runs panel remains present, scrolls internally when its content exceeds the available height, and keeps its bottom actions reachable. Load explicitly replaces the center detail.
- The selected and loaded Run use separate visual states at every supported width.
- The matrix may horizontally scroll only if enlarged text makes it necessary; row/column headers remain visible or repeated for accessibility.
- Required mocks: no Runs; mixed Live/Sequence Test list; Completed, Stopped, Interrupted, Failed; Run with model; Trigger Every Droplet/no model; Run with Unresolved values; full sequence present/absent; missing Droplet Log; unreadable Run Summary; Notes edit/save success; Notes save failure; long paths/names; 200% scaling.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.9, §§8, 10, 14–17 and D-005, D-013; *OpenDSS v2 Information Architecture and Screen Inventory* §§3.1, 4.15–4.16, §5, and §6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §14, §16, §§17–19; *OpenDSS Detailed User Workflow Specification* §22 and Run/file/persistence contracts as amended for Unresolved and Runs-only scope; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–10 as adapted table, master-detail, and accessibility evidence.

---

# 18. Settings design

## 18.1 Scope and purpose

Settings contains only:

```text
Storage
Application Information
Training Environment
Diagnostics
```

Its purpose is to manage the default data root, inspect local application/runtime facts, and provide the single in-application entry point for diagnosing or repairing the Training Environment. It must not duplicate Camera or DAQ settings and must not expose detector, Droplet Crop, routing, internal timing, training parameters, cloud, account, telemetry, update, or legacy-migration controls.

## 18.2 Layout

```text
Settings

STORAGE
Default Data Root: <path>
[ Choose Default Data Root ]  [ Open Data Root ]

APPLICATION INFORMATION
OpenDSS Version: <value>
Schema Versions: <values>
Runtime Availability: <facts>
Camera Driver Availability: <fact>
DAQ Driver Availability: <fact>
GPU Environment Availability: <fact>

TRAINING ENVIRONMENT
Status: <Ready | Missing | Broken | Checking>
Last checked: <local date/time | Never>
Result: <concise factual result>
[ Repair Training Environment ]

DIAGNOSTICS
Diagnostic Folder: <path>
[ Open Diagnostic Folder ]
```

## 18.3 Design contract

| Contract field | Specification |
|---|---|
| **Main user goal** | Set a valid default storage root, inspect application/runtime facts, and diagnose or repair the Training Environment. |
| **Major regions** | Storage; Application Information; Training Environment; Diagnostics; contextual fault banner. |
| **Dominant hierarchy** | Current data root and direct folder actions; version/runtime facts; Training Environment status and repair; diagnostic-folder access. |
| **Operation-side panel** | Not required. Content may use one centered settings column or grouped panels. |
| **Primary action** | Choose Default Data Root. |
| **Secondary actions** | Open Data Root; Repair Training Environment; Open Diagnostic Folder. |
| **Required artifact/hardware** | No hardware. A new root must validate as writable before becoming authoritative. |
| **Output** | Updated application storage preference. No scientific artifact. |
| **Direct disabled reasons** | Picker validation occurs after selection. Failure leaves prior root active and displays the direct path/permission reason. |
| **Fault banner** | Below Settings heading and above the affected group. |
| **Applicable presentations** | Ready, Failed. |
| **Next likely action** | Return directly to any workspace. |

## 18.4 Storage behavior

The first-release default data root is:

```text
%USERPROFILE%\Documents\OpenDropletSortingSuite
```

- The current default data root is shown as a path field with full-value inspection and middle elision.
- Choose Default Data Root opens a standard Windows folder picker.
- The selected location becomes authoritative only after writability validation and preference persistence succeed.
- A failed change leaves the prior valid root unchanged and states that fact.
- Changing the default root affects future operations only. It does not relocate existing artifacts or alter an active operation's snapshotted output location.
- Open Data Root invokes Windows Explorer.

## 18.5 Application information

Read-only facts include:

- OpenDSS application version;
- supported/active schema versions;
- application runtime availability;
- Camera driver/integration availability;
- DAQ driver/integration availability;
- GPU environment availability for automatic acceleration;
- other factual local prerequisites needed for troubleshooting.

Unavailable drivers are factual statuses, not Failed application state unless an attempted operation failed. No download or update action is added.

## 18.6 Diagnostics

Diagnostics provides the diagnostic folder path and Open Diagnostic Folder. A log/diagnostic stream MAY be added only when it reflects existing approved diagnostic data and does not turn normal workflows into console-driven operation.

## 18.7 Training Environment

The Training Environment group shows only three concise read-only facts: **Status**, **Last checked**, and **Result**. It contains one always-visible button whose exact label is **Repair Training Environment**. It contains no diagnostic-folder action; Diagnostics remains the separate group defined in §18.6.

Activating **Repair Training Environment** launches the application-owned **Set Up or Repair OpenDSS Training** tool immediately, without a confirmation dialog, while OpenDSS remains open. The Settings controller owns the single child-process lifecycle and prevents duplicate concurrent launches without hiding the button. The tool always diagnoses first. If the environment is already healthy, it changes nothing and reports exactly **Training environment is ready. No repair was needed.**

When the tool exits, whether successfully, unsuccessfully, or after cancellation, OpenDSS automatically reruns the shared authoritative readiness diagnosis and refreshes **Status**, **Last checked**, and **Result**. It never infers success from process exit alone. Train and Model Test use that same readiness result for their navigation gates and, while disabled, direct the user to **Settings > Training Environment** rather than opening a runtime-failure page or attempting to launch the tool from disabled navigation.

The exact visual seam is `SettingsWorkspace.ui.qml`: properties `trainingEnvironmentStatus`, `trainingEnvironmentLastChecked`, and `trainingEnvironmentResult`, plus exported alias `repairTrainingEnvironmentButton`. `ShellSingleImage.qml` owns only the bindings, Settings navigation presentation, and button-to-controller connection. `SettingsController` owns diagnosis state, the tool launch/process lifecycle, exact result reporting, and exit-triggered recheck; `App/main.cpp` may supply only the installed tool path or construction-time process dependency. No new page, overlay, confirmation, navigation control, diagnostic-folder action, process service, or generalized launcher is introduced.

## 18.8 Accessibility and mock states

- Group headings use semantic heading levels.
- Read-only values remain selectable/copyable where practical and are not styled as disabled.
- At minimum width, groups stack without changing structure.
- Required mocks: default root valid; long custom root; root selection failure; preference-write failure; Camera/DAQ drivers unavailable; CPU-only environment; GPU available; Training Environment Never/Checking/Ready/Missing/Broken and healthy no-op result; repair tool running/exit recheck; diagnostic folder missing/unopenable; 200% scaling.

**Source basis:** *OpenDSS Approved v2 Product Model* §7.10, §§11–12 and D-017; *OpenDSS v2 Information Architecture and Screen Inventory* §§1.4, 3.1, 4.17, and §8; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §15 and §16; *OpenDSS Detailed User Workflow Specification* §23 as narrowed by *OpenDSS Approved v2 Product Model* and applicable storage/diagnostic requirements; *OpenDSS Product Design Specification*, Draft v0.1 §§5–6 and §§9–10 as adapted form and accessibility evidence.

---

# 19. Bottom Configuration panel

## 19.1 Purpose and ownership

The shared slide-out Configuration panel is owned by the application shell and is the only location for user-editable Camera, DAQ, and approved Detector Configuration settings. Values are shared across all workspaces. Individual workspaces and Settings must not maintain duplicate settings.

Its visible panel heading and shell action are exactly `Configuration`. `Hardware` and `Hardware Configuration` MUST NOT be used as visible names. Existing internal component or controller names do not need a broad rename merely to satisfy visible terminology.

```text
Header: Camera: Streaming | DAQ: Ready | Active Model: … | Activity: … [Configuration]

                                             ┌──────────────────────────────┐
                                             │ CONFIGURATION                │
                                             │                              │
                                             │ CAMERA                       │
                                             │ Status: <status>             │
                                             │ <qualified supported fields> │
                                             │                              │
                                             │ DAQ                          │
                                             │ Status: <status>             │
                                             │ Output Channel [ value ▼ ]   │
                                             │ <qualified supported fields> │
                                             │                              │
                                             │ DETECTOR CONFIGURATION       │
                                             │ Minimum Size <value> px² [Set]│
                                             │                              │
                                             │ [ Close ]                    │
                                             └──────────────────────────────┘
```

`<qualified supported fields>` means only properties exposed by the integrated hardware adapter. Generic placeholder controls are prohibited.

## 19.2 Open and closed treatment

- The header Configuration action opens or closes the panel.
- The panel overlays from the bottom-left while the global status header remains visible.
- The ordinary open/closed presentation may remain stable during in-session navigation.
- It is not a workspace or navigation item.
- A scrim MAY be used over the workspace when needed for focus clarity, but it should be light enough to preserve operation context and must not imply that the application is blocked when the panel is editable.
- Escape closes the panel when permitted. A visible Close action is always available.
- During Live Starting, Running, Paused, and Stopping, the panel remains openable for Detector Configuration while Camera and DAQ sections remain locked.

## 19.3 Panel section anatomy

Each Camera or DAQ section contains:

1. section heading and device icon;
2. current availability/ownership status in text;
3. optional device name or adapter fact;
4. qualified editable controls when available and idle;
5. inline validation/persistence feedback;
6. lock-owner explanation when read-only.

Camera and DAQ sections remain independently usable except during Live, which locks those two sections while leaving Detector Configuration available.

### 19.3.1 Detector Configuration

Detector Configuration contains exactly one approved detector control, presented on one line:

```text
Minimum Size    <current integer value> px²    [ Set ]
```

- `Set` is enabled only when the current workspace presents a visible frame.
- When enabled, `Set` enters a rectangle-draw mode in that currently visible frame workspace. Escape cancels the mode and returns focus to `Set`.
- The drawn rectangle is mapped through the displayed-frame transform into source-image pixels.
- Fractional mapped source-image rectangle dimensions are rounded to the nearest whole source pixel before area is calculated; the minimum contour-area threshold is the resulting integer width × integer height in `px²`.
- The rectangle's source-pixel area becomes the existing detector minimum contour-area threshold. Rectangle width and height are not separate thresholds, and no new width/height rejection, detector mode, or parallel detector state may be invented.
- The rectangle is an input gesture, not a second persisted geometry setting. The one authoritative applied value is the existing minimum contour-area threshold expressed in source pixels.
- The initial authoritative minimum contour-area threshold is exactly `100 px²`. The product MUST NOT present or retain `Automatic` or `-1` as its initial/default state.
- Loading a legacy detector minimum-area value of `-1` converts it to the authoritative numeric value `100 px²`; `-1` is not retained in product state.
- When no frame view exists, `Set` is disabled with **No frame is available**.
- The control remains editable during an active Run. A successfully committed change applies immediately to subsequent detector processing.
- An in-run change MUST NOT be written to logs, `events.csv`, Run summaries, Results, or any other Run file. No history, timestamp, frame association, or Run provenance may be inferred or added.
- Setup Profile persistence stores only the current minimum contour-area threshold value.

This UI exposes the existing qualified detector threshold; it does not authorize replacement or behavioral modification of protected detector mechanics. Implementation must reuse the current authoritative threshold boundary and satisfy protected-asset change control with the smallest direct adapter needed by the current consumer.

### 19.3.2 Camera default and LUT

- Bit Depth is exactly `8-bit` only for a new/default live-Camera state. A legacy live-Camera profile with no Bit Depth field resolves to `8-bit`. Loading a saved supported Setup Profile preserves its explicit saved Bit Depth and MUST NOT overwrite that value with the default. Loaded image files and sequences retain their native bit depth and are never converted by this live-Camera default.
- Camera exposes one `Auto` button beside the existing manual Exposure input. One click performs one one-shot auto-exposure operation through the existing camera/exposure path; success applies the calculated exposure and updates the same input, which displays no more than two digits after the decimal point. Failure preserves the prior usable exposure and does not wedge the UI. Manual Exposure remains unchanged. Continuous auto exposure, additional settings, new camera abstractions, protected DCAM behavior changes, and unrelated refactoring are not authorized.
- The visible section title is exactly `LUT`. Retain the existing preview-only LUT slider and required LUT behavior. Remove the displayed numeric row beneath it; the slider retains its accessible name, role, and current value without a replacement visual numeric readout.

### 19.3.3 DAQ numeric-step responsiveness

The existing `+` and `−` controls for voltage, frequency, duration, and delay MUST remain responsive during repeated interaction. Intermediate edits are coalesced and applied asynchronously through the existing DAQ owner; synchronous or repeated hardware, persistence, validation, or other expensive work MUST NOT block the UI thread for each intermediate step. The final accepted value is guaranteed to be applied exactly and reflected as authoritative. A rejected or failed apply restores the last successfully applied value and exposes the direct reason. Existing units/ranges/step semantics, ownership and active-run locks, waveform behavior, limits, Stop-to-zero, and every DAQ safety invariant remain unchanged.

## 19.4 Immediate-apply behavior

There is no global Apply button.

A valid change applies when committed:

- selector: on selection;
- checkbox/toggle: on change;
- numeric/text value: on Enter or focus commit after validation.

On success:

- the new applied value becomes authoritative;
- all workspaces observe the same value;
- future operation snapshots use it;
- a subtle `Applied` confirmation MAY appear at field or section level.

On rejection:

- the control returns to the last successfully applied value;
- a direct field-level reason is shown;
- the authoritative value and any active operation snapshot remain unchanged;
- focus remains on the field or moves to its validation message according to platform accessibility behavior.

A transient draft that has not been committed must be visually distinct from the applied value if the control permits such a state.

## 19.5 Panel conditions

| Condition | Panel presentation | Editable behavior | Direct explanation |
|---|---|---|---|
| **Idle and available** | Section visible and enabled. | Valid edits apply immediately. | None required. |
| **Unavailable** | Panel opens; unavailable section remains visible with factual status. | All controls in that section disabled. Other available/idle section remains editable. | **Camera unavailable** or **DAQ unavailable**. |
| **Owned by non-Live operation** | Panel may open. Owned section is read-only; unowned section remains independently editable. | No changes to owned device. | **Camera settings are locked while Droplet Dataset Capture is active**, or equivalent owner. |
| **Live Starting/Running/Paused/Stopping** | Configuration remains openable; Camera and DAQ sections are locked; Detector Configuration remains available. | No Camera or DAQ edit. Minimum Size remains editable and applies immediately after successful commit. | **Camera and DAQ settings are locked while sorting is active** or **The current Run is paused**. |
| **Starting or Stopping non-Live operation** | Same ownership as accepted operation. | Owned section read-only. | Operation-specific starting/stopping message. |

## 19.6 Resource ownership effects

| Operation/action | Camera section | DAQ section | Detector Configuration |
|---|---|---|---|
| Single Image | Briefly locked | Available if idle | Editable when a frame is visible |
| Image Sequence | Locked | Available if idle | Editable when a frame is visible |
| Droplet Dataset Capture | Locked | Available if idle | Editable when a frame is visible |
| Training | Available if idle | Available if idle | Set disabled; no frame view |
| Model Test | Available if idle | Available if idle | Set disabled; no frame view |
| Sequence Viewer | Available if idle | Available if idle | Editable when a frame is visible |
| Label / Library / Results / Settings | Available if idle | Available if idle | Set disabled unless the current workspace has a visible frame |
| Live | Locked | Locked | Editable, including during an active Run |
| Sequence Test, Physical DAQ Output on | Available if idle | Locked | Editable when a frame is visible |
| Sequence Test, Physical DAQ Output off | Available if idle | Available if idle | Editable when a frame is visible |
| Send Test Pulse | Unaffected | Brief momentary use; control feedback only | Unaffected |

## 19.7 Prohibited panel content

The panel MUST NOT expose:

- droplet-detection parameters other than the approved `Minimum Size` control mapped directly to the existing minimum contour-area threshold;
- Droplet Crop parameters beyond the fixed artifact contract;
- width-based or height-based small-droplet rejection;
- routing-algorithm parameters;
- internal tracking or synchronization timing;
- training parameters;
- a software arming state;
- Hit Class;
- Decision Boundary;
- Setup Profile management as a list/library.

DAQ Output Channel is a DAQ technical setting in the panel and remains distinct from the Decision Boundary in Live/Sequence Test.

## 19.8 Relationship to the global header

- Header values update from the same authoritative hardware state as the panel.
- Selecting the Camera or DAQ header status MAY focus the corresponding panel section when opening is permitted, but it must not create a second settings surface.
- When Camera or DAQ is locked, the header remains factual and the Configuration action communicates ownership while preserving access to Detector Configuration.
- Status symbols and section badges use the same semantic vocabulary: Unavailable, Connected/Streaming for Camera; Unavailable, Ready/Active for DAQ.

## 19.9 Focus containment and keyboard operation

- On open, focus moves to the panel heading or first enabled control in the requested section.
- Tab and Shift+Tab remain within the panel until it closes.
- Escape closes when not forced closed/locked by Live.
- Arrow keys operate selectors and numeric steppers according to control conventions.
- Enter commits validated text/number fields.
- When the panel closes, focus returns to the Configuration action or the header item that opened it.
- After Live Start locks Camera and DAQ sections, focus moves to the Live Starting status; the Configuration action remains available for Detector Configuration.
- Locked fields remain discoverable in the accessibility tree with read-only/disabled state and owner explanation.

## 19.10 Collapse and responsive behavior

- Camera, DAQ, and Detector Configuration sections MAY independently collapse, but headings, status, current Minimum Size value, and applicable lock explanation remain visible.
- At minimum height, the panel scrolls internally while Close and current section heading remain accessible.
- At minimum width, it may occupy up to 44% of content width. It must not become a full-screen replacement that hides the global header.
- Preserve the current default owning-panel width unless the one-line `Minimum Size <value> px² [ Set ]` presentation demonstrably clips at a supported layout. If adjustment is required, use only the smallest default-width increase that resolves that factual clipping.
- Field labels must not truncate at 200% scaling; values may elide with full text available.

## 19.11 Minimum mock states

Idle Camera/DAQ available; Camera unavailable/DAQ Ready; Camera Streaming; DAQ Active; invalid Camera number; rejected DAQ channel; Camera locked by Image Sequence; Camera locked by Droplet Dataset Capture; DAQ locked by Sequence Test; Sequence Test with DAQ off; Live Camera/DAQ lock with Detector Configuration editable; Live paused with Detector Configuration editable; Minimum Size Set with a visible frame; Set disabled with no frame; display-to-source rectangle mapping; immediate in-run threshold update with no Run-file provenance; new/default Camera at 8-bit; saved explicit non-8-bit profile preserved; LUT without displayed numeric values; responsive repeated DAQ `+`/`−`; Starting/Stopping lock; long device names; unsupported property absent rather than disabled placeholder; 200% scaling; focus return after close.

**Source basis:** *OpenDSS Approved v2 Product Model* §5.2, §§11–13 and D-002, D-016; *OpenDSS v2 Information Architecture and Screen Inventory* §§1.6–1.7 and §6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §5, §17, and applicable workspace ownership rules; *OpenDSS Detailed User Workflow Specification* hardware, concurrency, and error requirements as amended by *OpenDSS Approved v2 Product Model*; *OpenDSS Product Design Specification*, Draft v0.1 §§4–6, §8 contextual-panel evidence, and §§9–10 as adapted.

---

# 20. Setup Profile design

## 20.1 Purpose and file model

A Setup Profile is one ordinary OpenDSS v2 JSON file used in Live pre-run. The required actions are:

```text
Open Profile
Save Profile
Save Profile As
```

There is no managed profile list, Import, Export, Delete, archive, migration, or legacy-conversion workflow. Users manage copies, renames, moves, and deletion through ordinary Windows file ownership.

## 20.2 Profile control group

The Live pre-run panel SHOULD use this compact group:

```text
SETUP PROFILE
<No profile loaded> or <filename.json>
<Loaded / Modified / values not applied summary>

[ Open ]  [ Save ]  [ Save As ]
```

The loaded profile indication includes:

- filename and elided full path;
- supported v2 schema state;
- `Modified` marker when current profile-covered values differ from the last loaded/saved snapshot;
- concise unapplied/missing-reference facts when applicable.

`Modified` is a file-difference indicator, not an unsaved scientific-operation state and not a managed-library status.

## 20.3 Profile content and interaction boundary

A Profile may contain:

- Camera settings;
- DAQ settings;
- current `Minimum Size` minimum contour-area threshold in source pixels;
- Active Model reference;
- Trigger Every Droplet;
- Hit Class;
- Live Decision Boundary Top-is-Hit/Bottom-is-Hit mapping; clicked X and Y coordinates are workspace-local and MUST NOT be written to the Profile;
- Record Full Image Sequence preference;
- Run Name;
- default Save Location.

It may not contain any other user-editable detector value, rectangle geometry, Droplet Crop parameter, routing-algorithm value, internal timing value, or training value. Notes and Experiment Type remain current Run fields unless a later approved file contract says otherwise.

The profile group does not duplicate editable Camera, DAQ, or Detector Configuration controls. It reports which values were applied; authoritative current values remain visible in the bottom Configuration panel.

## 20.4 Open Profile

Open uses a standard Windows file picker and is available only in Live pre-run. Opening performs these steps:

1. identify a supported v2 Setup Profile schema;
2. parse readable profile-covered values;
3. resolve the referenced Model Package when present;
4. apply valid Camera/DAQ values immediately only when the corresponding device is available and idle;
5. load readable nonhardware Run selections into Live pre-run;
6. update the global Active Model only when the referenced package is valid and replacement is not locked;
7. report any missing, locked, unavailable, or unapplied values directly.

A successful open does not Start Sorting.

## 20.5 Missing-model reference

When a referenced Model Package is missing or unreadable:

- all other readable values load;
- the missing model path/reference is shown factually;
- the current Active Model is not silently substituted;
- Class-Based Sorting remains unavailable until a valid model is selected;
- Trigger Every Droplet may proceed without a model;
- the profile group displays `Model reference unavailable` and, where useful, a direct Select/Open Library action.

## 20.6 Hardware unavailable or locked

A profile may be opened and inspected when hardware is unavailable or locked by another operation. In that case:

- readable nonhardware Run selections load;
- authoritative applied hardware values remain unchanged;
- the pre-run profile summary reports, for example:
  - `Camera profile values not applied — Camera unavailable`;
  - `DAQ profile values not applied — DAQ is in use by Sequence Test`;
- the profile's hardware values are not exposed as a second editable settings surface;
- the user may reopen the profile or apply equivalent values through the hardware panel after the device becomes available and idle.

## 20.7 Invalid or unsupported file

- An unsupported schema is not partially interpreted. Current valid configuration remains unchanged, and the direct message is **Unsupported OpenDSS v2 schema**.
- A supported schema with a missing model or temporarily unapplied hardware values may load other readable values as described above.
- Malformed values inside an otherwise supported file are reported by field/category. The application may load independent readable values only where the approved loader contract permits and where doing so cannot silently replace an authoritative valid value.
- The summary must distinguish `Loaded`, `Partially loaded`, and `Not loaded` in text.

## 20.8 Save Profile and Save Profile As

Save writes the current profile path. When no path exists, it behaves as Save As.

Save and Save As snapshot:

- current authoritative applied Camera/DAQ settings;
- current authoritative `Minimum Size` minimum contour-area threshold;
- current Active Model reference when present;
- current approved Live pre-run selections covered by the Profile.

They must not save unsupported drafts as though they were applied values. If a Camera, DAQ, or Detector Configuration edit is invalid, the last successfully applied value is saved. Rectangle geometry is not saved.

On success, the profile name/path and Modified marker update. On failure, the prior file remains authoritative and one direct file reason is shown.

## 20.9 Focus, keyboard, and mock states

- Profile controls occur first in the Live pre-run Tab order.
- Open returns focus to the profile summary after load; validation messages are reachable next.
- Save As uses the standard Windows save interaction.
- Elided paths expose full text to assistive technology.
- Required mocks: no profile; loaded valid profile; modified profile; Save with no path; Save As; missing model; invalid model package; Camera unavailable; DAQ locked; partial load; unsupported schema/no load; malformed supported value; save permission failure; long filename/path; profile loaded with Trigger Every Droplet and no model.

**Source basis:** *OpenDSS Approved v2 Product Model* §9, §11, and D-009; *OpenDSS v2 Information Architecture and Screen Inventory* §3.2, §5, and removed/hidden interface elements; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §12.3, §16, and profile-related fault/ownership rules; *OpenDSS Detailed User Workflow Specification* §20 and §30 only where not superseded by the ordinary-file, narrowed-content model; *OpenDSS Product Design Specification*, Draft v0.1 contextual file-control evidence only.

---
# 21. Contextual workflow links

## 21.1 Interaction model

Contextual workflow links perform only two actions:

1. navigate to an approved persistent workspace;
2. preselect the named artifact when it remains readable and compatible.

They MUST NOT start the destination operation, create a project, lock the user into a wizard, prevent direct navigation, or change unrelated global state.

## 21.2 Required links

| Source | Link label | Destination | Preselection and presentation |
|---|---|---|---|
| Droplet Dataset Capture Completed or recoverable Interrupted | **Open in Label** | Data > Label | Selects the completed/recoverable `dataset.json`; Label derives Empty/Unavailable/Ready from the authoritative Dataset loader. |
| Label | **Use in Train** | Models > Train | Selects the current Dataset. The user must separately select an existing Library-defined model identity in Train. |
| Train after successful Model Package save | **Open in Model Test** | Models > Model Test | Selects the new Model Package; the source Dataset may remain selected when still available. Does not make Model Test mandatory. |
| Model Library | **Open in Model Test** | Models > Model Test | Selects the current Model Package without changing Active Model. |
| Image Sequence Completed | **Open in Sequence Viewer** | Data > Sequence Viewer | Selects `sequence.json` and displays the first readable frame. |
| Image Sequence Completed | **Open in Sequence Test** | Sort > Sequence Test | Selects `sequence.json`; Trigger Every Droplet, Model, Hit Class, Decision Boundary, and physical-output choice remain explicit. |
| Live post-operation | **Open Run Summary** | Results > Runs | Selects the newly finalized or recoverable Live Run. |
| Sequence Test post-operation | **Open Run Summary** | Results > Runs | Selects the newly finalized or recoverable Sequence Test Run. |
| Selected Run with full Image Sequence | **Open Saved Sequence** | Data > Sequence Viewer | Selects the Run's sequence. The action is absent or directly unavailable when no full sequence exists. |

## 21.3 Visual treatment

- Contextual links are secondary actions with a directional/open icon and full text.
- The link label names the destination or artifact, not a vague `Next` action.
- In completion presentations, the most likely link may become the local primary action, such as Open in Label or Open Run Summary.
- Links must not be arranged as a numbered wizard or progress tracker.
- When preselection fails, the destination workspace remains open and shows its normal direct artifact error. The source workspace does not fabricate success.
- A destination may replace the preselected artifact before Start.

## 21.4 Focus behavior

After activation, focus moves to the destination workspace title or selected-artifact summary. It must not move directly to Start, Save, Delete, or another consequential action. The destination's selected navigation state updates immediately.

**Source basis:** *OpenDSS Approved v2 Product Model* §§4, 6, and 8; *OpenDSS v2 Information Architecture and Screen Inventory* §5; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §4.8 and §18; *OpenDSS Detailed User Workflow Specification* contextual handoffs in §§12–22 as amended by *OpenDSS Approved v2 Product Model*. *OpenDSS Product Design Specification*, Draft v0.1 is not authoritative for workflow links.

---

# 22. Keyboard, focus, and repeated-work behavior

## 22.1 Keyboard-accessibility baseline

Every core workflow MUST be fully operable by keyboard. Pointer interaction may optimize direct manipulation, but no required action may depend on hover, drag, or a context menu without an equivalent keyboard path.

Keyboard behavior must preserve these rules:

- visible focus on every interactive element;
- logical Tab order matching visual/task order;
- no global single-letter or number shortcut while typing in a field;
- no unmodified global key that starts or stops physical output;
- Escape closes menus, tooltips, dialogs, and the Configuration panel when permitted; it also cancels an active rectangle-draw mode without changing the applied threshold and does not silently cancel or Stop an operation;
- focus does not jump on routine data refreshes, counter updates, frame playback, or hardware polling;
- selection and keyboard focus remain visually distinct.

## 22.2 Major-region navigation

`F6` SHOULD cycle among major shell regions:

1. global status header;
2. primary navigation;
3. workspace content;
4. operation-side panel or selected-item inspector;
5. contextual fault banner when present.

`Shift+F6` SHOULD reverse the order. A region that is collapsed or absent is skipped. Cycling never changes the current artifact or operation.

## 22.3 Shortcut discovery

Shortcuts SHOULD be discoverable through:

- tooltips that append the shortcut;
- accessible descriptions;
- a workspace-local `Keyboard shortcuts` help action;
- button/menu text using platform-consistent accelerator notation;
- the component gallery and user documentation.

The normal workspace should not display a permanent dense shortcut legend unless repeated work benefits directly, as in Label.

## 22.4 Global versus workspace scope

### Global scope

Only navigation and shell-management shortcuts should be global. Recommended examples are:

| Shortcut | Scope and behavior |
|---|---|
| `F6` / `Shift+F6` | Cycle major shell regions. |
| `Ctrl+Shift+H` | Open/close Configuration. It remains available during Live; locked Camera/DAQ controls remain read-only. |
| `Esc` | Close current nonoperation overlay or cancel a local edit according to component behavior. |

Direct domain-navigation accelerators MAY be added if they are fully documented and do not conflict with field entry or assistive technology. They must open the approved workspaces only.

### Workspace scope

Workspace shortcuts become active only when focus is inside that workspace and no text, number, path, or combo-box editing session is active. Owning-workspace operation shortcuts are inactive after navigation away.

## 22.5 Field-editing behavior

- While a text/number/path field is editing, printable keys, Space, Delete, Backspace, arrows, Home/End, and standard editing chords belong to the field.
- Enter commits when the field contract defines commit-on-Enter; otherwise it activates the default dialog action only outside multiline fields.
- Escape cancels an uncommitted field edit and restores the last valid value where supported.
- Tab commits a valid field before moving focus. Invalid input keeps the authoritative applied value unchanged and exposes validation without trapping the user indefinitely.
- Class Name, Notes, and other multiline/editable text suppress Label/viewer single-key shortcuts.

## 22.6 Label repeated-work shortcuts

The default Label mapping SHOULD be:

| Shortcut | Action | Availability |
|---|---|---|
| `1` | Assign Class ID 0 | Crop collection/inspector focus; not while typing |
| `2` | Assign Class ID 1 | Same |
| `3` | Assign Class ID 2 | Three-class Dataset only |
| `S` | Skip selected crop(s) | Valid writable selection |
| `X` | Remove selected crop(s) from Dataset | Valid writable selection |
| `R` | Restore selected Removed crop(s) | Removed selection |
| `Ctrl+Z` | Undo last Label edit | Undo available |
| Arrow keys | Move focused crop | Crop collection focus |
| `Shift` + Arrow | Extend selection | Crop collection focus |
| `Space` | Toggle focused crop in selection | Crop collection focus |
| `Ctrl+A` | Select all visible/filter-matching crops | Collection focus; scope clearly announced |
| `Enter` | Move to selected-crop inspector or detailed preview | Crop selected |

The class action's accessible name includes current Class Name, for example `Assign Class 1, Single cell`.

## 22.7 Viewer shortcuts

The default Sequence Viewer and image-inspection mapping SHOULD be:

| Shortcut | Action |
|---|---|
| `Left` / `Right` | Previous/next frame in Sequence Viewer; no effect on Live/Capture operations. |
| `Left` / `Right` | Previous/Next Frame. |
| `Home` / `End` | First/last frame. |
| `Page Up` / `Page Down` | Larger seek. |
| `F` | Fit image to viewer. |
| `1` | Actual-pixel 1:1 view. |
| `+` / `-` | Zoom in/out. |
| Arrow keys while panned | Pan when focus is on the viewer and playback is paused. |

A viewer must expose a nonshortcut control for every action.

## 22.8 Pause, Resume, and Stop behavior

- Pause, Resume, and Stop remain explicit text-labeled controls in the owning operation panel.
- Pause and Resume occupy the same visual/Tab position when state changes.
- Stop remains adjacent but visually secondary to Pause/Resume while Running/Paused.
- A two-key accelerator for Pause/Resume MAY be provided and displayed; no unmodified Space behavior is used for Capture or Live operations because Space belongs to field entry and Sequence Viewer.
- Stop MUST NOT be bound to Escape.
- If a Stop accelerator is provided, it must be a deliberate chord, shown in the UI/documentation, scoped to the owning workspace, suppressed during field entry, and followed by the same idempotent Stop behavior as the button. It must not be described as Emergency Stop.
- On Stop acceptance, focus moves to the Stopping status rather than another enabled action.

## 22.9 Pane resizing and collapse

Every resizable splitter MUST provide:

- pointer drag;
- keyboard focus;
- arrow-key resizing in small increments, with a modifier for larger increments;
- an explicit Collapse/Expand action when collapse is supported;
- a Reset size action where practical;
- an accessible value or description of affected panes.

Collapsing a pane moves focus to the control that can restore it. Content selection and scroll position remain stable.

## 22.10 Dialog focus

- Initial focus goes to the safest non-destructive action or first required field.
- Destructive confirmation names the affected Model Package and consequence.
- Enter invokes the default action only when focus is not in a multiline field and the action is enabled.
- Escape activates Cancel/Close where safe.
- On dismissal, focus returns to the invoking control or affected list row.
- A destructive action must not receive default focus merely because it is visually prominent.

## 22.11 Focus after dynamic state changes

| Transition | Focus destination |
|---|---|
| Artifact opened successfully | Selected-artifact summary or first meaningful content heading |
| Start accepted | Starting status heading, then operation controls when available |
| Running begins | Operation status; user may Tab to Pause/Stop |
| Pause accepted | Paused status; Resume occupies prior Pause location |
| Completion | Outcome heading, then primary contextual action |
| Fault | Banner heading once; subsequent updates do not steal focus |
| Configuration closed | Configuration action or opener |
| Item deleted | Nearest remaining list row or Empty state |
| Filter changes | Filter control unless user explicitly moved to results |

**Source basis:** *OpenDSS Approved v2 Product Model* operation, fault, and accessibility-relevant boundaries in §§5, 13–14; *OpenDSS v2 Information Architecture and Screen Inventory* shell/workspace behavior in §§1–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§2, 4–18 and acceptance checklist; *OpenDSS Detailed User Workflow Specification* §§9, 14, 18, 21, 38–43; *OpenDSS Product Design Specification*, Draft v0.1 §§9–10 as adapted keyboard and accessibility evidence.

---

# 23. Responsive, high-DPI, and accessibility requirements

## 23.1 Supported desktop presentation

OpenDSS v2 targets desktop use on Windows 11.

| Reference | Window state | Design use |
|---|---|---|
| **Maximized** | Current display's full available work area | Startup and primary validation state. Balanced navigation, viewer, and operation-panel proportions. |
| **Restored minimum** | Exactly 1600 × 900 through the application's native Qt minimum-size clamp | Supported design, GUI, Qt Design Studio, and Computer Use validation state. The current Qt logical-unit implementation is acceptable; no separate physical-pixel interpretation or acceptance gate exists. The window may grow larger but never resizes below this minimum. |

Supported Windows scaling factors:

```text
100%, 125%, 150%, 200%
```

The application structure remains fixed in the maximized validation state. Display size does not introduce a mobile shell, alternate navigation, or different workspace model.

## 23.2 Responsive layout rules

### Maximized layout

- Expanded or compact navigation may be user-selected.
- Operation-side panel remains visible for operational workspaces.
- Secondary inspectors may collapse.
- Tables prioritize required columns and allow user resizing.

## 23.3 Inspector and panel collapse order

When horizontal space decreases, collapse in this order unless a workspace section specifies otherwise:

1. optional metadata/provenance detail;
2. selected-item inspector that has an explicit restore action;
3. expanded navigation to compact navigation;
4. secondary metric columns into stacked groups.

The operation-side panel for an active operation, contextual fault banner, viewer/crop collection, and primary action must not be hidden automatically.

## 23.4 High-DPI requirements

- Layout uses logical pixels and Qt high-DPI scaling, not fixed physical-pixel coordinates.
- Icons must use vector assets or appropriate multi-resolution raster exports.
- One-pixel dividers must remain optically visible without becoming disproportionately thick.
- Focus indicators and target hit areas scale with the interface.
- Camera and source images render at correct aspect ratio; 1:1 mode communicates source-pixel mapping at the current scale.
- No required label, value, field, button, table header, status, or disabled reason clips at 200% scaling.
- Elision is allowed for long names/paths only when full content remains available through selection, tooltip, accessible description, or detail view.

## 23.5 Contrast

The approved design MUST meet or exceed:

- 4.5:1 contrast for normal text;
- 3:1 for large text where the applicable accessibility definition permits;
- 3:1 for meaningful non-text controls, component boundaries needed to identify state, and focus indicators;
- equivalent contrast on dark viewer surfaces.

Disabled text may use lower emphasis but must remain readable enough to identify the control and understand its relationship to the adjacent reason. Semantic colors must be tested in default, hover, pressed, selected, and focus combinations.

## 23.6 Pointer targets

- Absolute minimum target: 24 × 24 logical px with sufficient spacing.
- Balanced default: 32–40 logical px.
- Primary operational actions: 36–44 logical px or larger.
- Splitter visible line may be 1 px, but its interactive target is at least 8 px.
- Small thumbnail badges are not independent targets unless enlarged to meet the target requirement.

## 23.7 Non-color cues

Color must not be the only cue for:

- Camera/DAQ availability;
- Ready, Active, Paused, Completed, Interrupted, or Failed state;
- selected navigation/item;
- keyboard focus;
- Class ID;
- Labeled, Skipped, or Removed crop state;
- Active Model;
- Physical DAQ Output state;
- validation error or missing prerequisite.

Text, icons/symbols, structural markers, patterns, or labels must accompany color.

## 23.8 Accessible names and semantics

- Every icon-only action has an accessible name.
- Group headings and workspace titles expose semantic hierarchy.
- Status items expose label and value together, such as `Camera, Streaming`.
- Metric labels and values remain associated.
- Tables expose row and column headers.
- Thumbnails expose crop ID, Label/Class Name, state, and selected state without requiring image interpretation.
- Viewer announces current frame and total only on explicit navigation or suitably throttled playback updates.
- Fault banners expose heading, reason, preservation result, and action labels in order.
- Custom controls must implement the corresponding Qt accessibility roles and states rather than relying on drawn appearance alone.

## 23.9 Enlarged text and clipping

At 200% scaling:

- buttons may grow vertically or wrap supporting text;
- primary action labels remain complete;
- direct disabled reasons wrap within their panel;
- header status values may elide long model/activity detail but labels remain identifiable;
- table rows may increase height;
- no horizontal clipping makes a field label indistinguishable;
- scrolling is permitted within content regions, not across the entire application shell in both axes.

## 23.10 Reduced motion

When reduced motion is active:

- panel and panel animations become immediate or short fades;
- progress remains perceivable through text/value changes;
- no pulsing semantic badge is required;
- auto-scrolling and animated reordering are avoided;
- sequence frame navigation remains user-controlled and is not disabled because it is core content.

## 23.11 Screen magnification and focus visibility

Focus must never be indicated only at the far edge of a large region while the focused control is elsewhere. When focus enters a scrollable panel, the focused element is scrolled into view without unexpected workspace-wide motion. Persistent operation controls should remain reachable under magnification.

**Source basis:** *OpenDSS Approved v2 Product Model* §§2–5 and 13–14; *OpenDSS v2 Information Architecture and Screen Inventory* shell and minimum-state requirements; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* detailed layouts and focus behavior; *OpenDSS Detailed User Workflow Specification* §§38–43; *OpenDSS Product Design Specification*, Draft v0.1 §10 and §12 as adapted responsive, high-DPI, contrast, target-size, and QA evidence.

---

# 24. Qt Design Studio and design-handoff contract

## 24.1 Handoff purpose

The design handoff must allow visual and interaction-state work to remain editable in Qt Design Studio without placing production business logic, hardware ownership, persistence, or scientific processing inside designer-authored forms.

This section defines a design boundary, not production architecture.

## 24.2 Form, wrapper, and view-model separation

| Layer | Responsibility | Must not contain |
|---|---|---|
| **Editable `.ui.qml` form** | Layout, visual hierarchy, component instances, token references, state preview hooks, nonprogrammer-editable text/icon/spacing properties | Hardware calls, file I/O, threading, navigation authority, operation-state mutation, scientific algorithms |
| **Behavior wrapper `.qml`** | Maps view-model properties/commands to the form, handles purely presentational interactions, focus routing, and component composition | Duplicate domain truth or independent operation state |
| **C++ or application-state view model** | Exposes authoritative projections, commands, validation results, locks, progress, counters, and accessible data | Screen-local literal styling or layout geometry that belongs in tokens/forms |
| **Domain/application services** | Existing authoritative hardware, Dataset, Sequence, Model, Training, Run, operation, and settings behavior | UI-specific visual state ownership |

Exact class names and production boundaries remain engineering choices, provided one authoritative owner per domain is preserved.

## 24.3 Centralized design tokens

Tokens MUST be centralized for:

- brand, surface, text, border, semantic, class, and viewer colors;
- typography roles and tabular-number settings;
- spacing, control heights, radii, divider widths, elevation, and motion duration;
- icon sizes and target sizes;
- navigation, header, operation-panel, panel, and inspector geometry recommendations;
- focus-ring treatment;
- breakpoints or responsive presentation rules.

Screen-local literal colors and arbitrary repeated geometry values should be treated as defects. Tokens may expose semantic aliases rather than raw values to support refinement.

## 24.4 Shared component structure

A recommended design-time organization is:

```text
ui/
├── Theme/
│   ├── ColorTokens.qml
│   ├── TypeTokens.qml
│   ├── SpacingTokens.qml
│   └── MotionTokens.qml
├── Components/
│   ├── Shell/
│   ├── Controls/
│   ├── Data/
│   ├── Viewer/
│   ├── Feedback/
│   └── Operation/
├── Screens/
│   ├── Capture/
│   ├── Label/
│   ├── SequencePlayer/
│   ├── Train/
│   ├── ModelTest/
│   ├── ModelLibrary/
│   ├── Live/
│   ├── SequenceTest/
│   ├── Results/
│   └── Settings/
├── MockData/
│   ├── Fixtures/
│   ├── StateCatalog/
│   └── ScreenScenarios/
└── Gallery/
    ├── ComponentGallery.ui.qml
    └── ScreenStateGallery.ui.qml
```

The repository may use different directories, but the separation of tokens, components, screens, mocks, and galleries must remain clear.

## 24.5 Design-time mock data

Every screen form MUST be previewable without connected hardware, live files, Python processes, or an active application backend. Mock data should expose typed properties equivalent to production view-model projections, including:

- statuses and ownership;
- artifact names, paths, class definitions, counts, and errors;
- operation lifecycle states;
- counters, progress, and metrics;
- enabled states and direct disabled reasons;
- banner content and recovery actions;
- selected, focused, Active Model, Labeled, Skipped, and Removed states.

Mocks must not become hardcoded production defaults.

## 24.6 Component gallery

The component gallery MUST show:

- every component and variant from Section 6;
- default, hover, pressed, focus, disabled, busy, invalid, read-only, selected, and locked states where applicable;
- light and dark-viewer contexts;
- compact, balanced, and enlarged-text presentations;
- two-class and three-class tokens;
- semantic state combinations;
- long names/paths and empty values;
- accessible-name and target-size annotations for review.

## 24.7 Screen-state previewing

Each workspace form MUST expose a deterministic state selector for the applicable states in Section 8. Design reviewers must be able to preview:

- Empty/Unavailable/Ready;
- each applicable lifecycle phase;
- long/edge-case content;
- fault and preservation banners;
- maximized full-available-work-area geometry;
- 100%, 125%, 150%, and 200% scaling references.

State selectors exist only in design/mock builds and must not appear as product controls.

## 24.8 Nonprogrammer-editable properties

Without editing C++ or production application logic, a qualified designer should be able to:

- change tokens and typography roles;
- adjust panel padding, spacing, and recommended widths;
- move or regroup presentational regions within the approved workspace contract;
- replace approved icons and logo assets;
- edit user-facing explanatory text;
- preview all required states and mock data;
- adjust focus visuals, dividers, and surface treatment;
- inspect compact/expanded navigation and inspector collapse presentations.

A designer must not be able to alter navigation authority, operation prerequisites, resource ownership, scientific terminology, artifact contracts, or state transitions merely by editing a form.

## 24.9 Repository relationship

The existing `OpenDSS_clean` repository is compatible implementation evidence because it contains a C++/Qt/CMake application, a desktop application area with resources/styles/widgets, runtime Camera/DAQ/detection/metadata/ONNX components, training Python packages, and branding/screenshot assets. These assets may inform feasibility and reuse after technical review.

They must not override this specification. Historical repository behaviors such as old navigation, user-selected training environments/devices, legacy capture labels, detector controls, or diagnostic modes are not carried forward merely because code or screenshots exist.

## 24.10 Handoff package

The design handoff SHOULD include:

- this approved specification after review;
- centralized token source;
- component gallery;
- editable screen forms;
- wrappers or documented property/command interfaces;
- complete mock-data/state catalog;
- approved SVG icon and brand assets;
- accessibility annotations;
- screenshot references at supported sizes/scales;
- visual-regression baselines;
- a design QA checklist and traceability matrix.

**Source basis:** *OpenDSS Approved v2 Product Model* §18 and repository-as-evidence boundary; *OpenDSS v2 Information Architecture and Screen Inventory* §6 and §7; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §3.2 and §22–23; *OpenDSS Detailed User Workflow Specification* §§73–78; *OpenDSS Product Design Specification*, Draft v0.1 §11 and §12 as adapted Qt Design Studio and handoff evidence; `OpenDSS_clean` repository structure as optional implementation evidence only.

---
# 25. Required mock-data and design-review states

## 25.1 Mock-data principles

Mock data is a required design artifact, not decorative sample content. It MUST be deterministic, locally available, and sufficient to preview every applicable state without production hardware, files, or runtime services.

Mock fixtures MUST:

- use approved terminology and realistic two-class/three-class structures;
- include short, typical, and intentionally long names, paths, notes, and values;
- identify whether data is factual mock content, not live laboratory output;
- preserve relationships among selected artifacts, Active Model, hardware state, locks, and operation state;
- avoid scientific quality judgments or misleading pass/fail examples;
- include failure and partial-preservation results, not only ideal success;
- be reusable across component gallery, screen previews, screenshots, accessibility checks, and visual regression.

## 25.2 Global fixture catalog

At minimum, the design system MUST provide these global fixtures:

| Fixture ID | Required content |
|---|---|
| `hardware.none` | Camera Unavailable; DAQ Unavailable; Current Activity Idle. |
| `hardware.cameraConnected` | Camera Connected but not Streaming; DAQ state independently variable. |
| `hardware.cameraStreaming` | Camera Streaming with typical frame facts. |
| `hardware.daqReady` | DAQ Ready with valid applied output channel/settings. |
| `hardware.daqActive` | DAQ Active and owned by Live or physical-output Sequence Test. |
| `hardware.invalidEdit` | Last valid applied value plus rejected draft and direct reason. |
| `model.noneActive` | No Active Model. |
| `model.twoClassActive` | Active two-class MobileNetV3-Small or EfficientNet-B0 package with Class IDs/Names. |
| `model.threeClassActive` | Active three-class package with Class IDs/Names. |
| `model.longName` | Valid package with a long user-facing name and long path. |
| `dataset.empty` | Dataset with class definition and zero crops where technically representable for visual review. |
| `dataset.noClasses` | Captured Dataset before class definition. |
| `dataset.partiallyLabeled2` | Two-class Dataset with Labeled and Unlabeled crops. |
| `dataset.partiallyLabeled3` | Three-class Dataset with Labeled and Unlabeled crops. |
| `dataset.mixedStates` | Labeled, Unlabeled, Skipped, Removed, and missing-file crops. |
| `dataset.large` | Virtualized collection with at least 100,000 mock crop entries and varied states. |
| `sequence.valid` | Readable multi-frame sequence with representative metadata. |
| `sequence.missingFrame` | Valid sequence metadata with one missing/unreadable middle frame. |
| `run.completedLive` | Completed Live Sorting Run with model and all count groups. |
| `run.interruptedLive` | Interrupted Live Run with preserved data and direct fault. |
| `run.failedSequenceTest` | Failed Sequence Test Run with partial folder facts. |
| `run.fullSequence` | Run containing a full saved Image Sequence. |
| `run.noSequence` | Valid Run without a saved full sequence. |
| `file.unsupportedSchema` | Unsupported v2/legacy artifact selection. |
| `file.unwritable` | Valid selected output folder that cannot be written. |
| `operation.otherActive` | Another long-running operation owns the global slot. |

## 25.3 Required global state combinations

The following combined states MUST be previewable because isolated component states are insufficient:

1. no hardware, no Active Model, application opened at Data > Capture with all three Capture sections collapsed;
2. Camera Streaming and DAQ Ready with no Active Model;
3. Camera Streaming, DAQ Ready, two-class Active Model;
4. Camera Streaming, DAQ Ready, three-class Active Model;
5. Image Sequence active while Camera settings are locked and DAQ settings remain editable;
6. Training active while the selected Dataset is read-locked but hardware remains editable;
7. Sequence Test active with physical DAQ output enabled, locking DAQ only;
8. Sequence Test active with physical output disabled, locking no hardware but occupying the global slot;
9. Live Running with the entire panel closed/locked;
10. Live Paused with Camera Streaming, DAQ Active, and Current Activity Paused;
11. faulted operation with preserved artifact and contextual recovery actions;
12. faulted operation with no usable output;
13. an Active Model package locked by Live while another package remains manageable;
14. a loaded Setup Profile with missing model reference and unapplied hardware values;
15. direct disabled reason changing as prerequisites are restored.

## 25.4 Shared component mock states

| Component | Minimum states to preview |
|---|---|
| Navigation item | Default, hover, selected, focus, selected+focus, compact, expanded, long label, high scaling. |
| Global status item | Each approved value, long Active Model name, unavailable, active, paused, elided detail. |
| Button | Primary/secondary/ghost/destructive; hover, pressed, focus, disabled with adjacent reason, busy, long label. |
| Icon button | Default, focus, selected, disabled, destructive; tooltip and accessible name annotations. |
| Capture disclosure sections | Three fixed headings; none, one, two, and three bodies expanded; independent scrolling; focused/collapsed behavior; active section forced open; other headings disabled; 200% text. |
| Text/number/path fields | Empty, populated, focus, invalid, read-only, disabled, busy commit, long path, unit, rejected immediate apply. |
| Combo box | Closed/open, long value, invalid, disabled, keyboard focus. |
| Checkbox/toggle | On/off/focus/disabled; Physical DAQ default on; Record Full Image Sequence off/on. |
| List row | Selected, Active Model, selected+Active, locked, unreadable, focus, long metadata. |
| Data table | Empty, populated, selected row, focus, unreadable row, large values, horizontal constraint, 200% scaling. |
| Dataset thumbnail | Unlabeled, Labeled Class 0/1/2, Skipped, Removed, missing file, selected, focused, selected+focused, multi-selected. |
| Sequence timeline | Empty, beginning, middle, end, focus, large frame count, missing-frame stop. |
| Metric tile | Zero, typical, large, unavailable/not recorded, long unit/label, neutral scientific metrics. |
| Progress display | Indeterminate Starting, determinate early/mid/late, Stopping/finalizing, interrupted. |
| Status badge | Neutral, Unavailable, Ready, Active, Paused, Completed, Interrupted, Failed; non-color cue visible. |
| Empty state | No Dataset, no Sequence, no Models, no Runs; direct action where applicable. |
| Disabled reason | Short reason, long class mismatch, changing blocker, screen-reader description. |
| Inline validation | Required field, range error, rejected hardware value, permission error, corrected state. |
| Fault banner | Interrupted/preserved; Failed/partial; Failed/no output; one and two actions; long direct reason. |
| Dialog | Safe confirmation, destructive model deletion, long package name, focus order, 200% scaling. |
| Panel | Open idle, unavailable section, partial lock, full Live lock, invalid field, internal scroll, focus return. |
| Splitter | Default, resized, minimum pane, collapsed, keyboard focus, reset. |
| Tooltip | Icon meaning, elided path, compact navigation, disabled-reason duplication. |

## 25.5 Workspace state catalog

| Workspace/mode | Minimum design-review states |
|---|---|
| **Capture — Single Image** | No Camera; Camera Streaming Ready; busy capture; saved TIFF; write failure; another operation owns Camera. |
| **Capture — Image Sequence** | Ready blank Duration; Starting; Running; Paused; Stopping; Completed by Stop; Completed by Duration; Interrupted recoverable; Failed; Camera section locked/DAQ editable. |
| **Capture — Droplet Dataset Capture** | No Active Model but Ready; fixed processing unavailable; Running counters; Paused; Completed; recoverable interruption; write failure; no detector/crop controls. |
| **Label** | No Dataset; no classes; two-class; three-class; partially labeled; mixed Skipped/Removed; bulk selection; locked Dataset; missing crop; autosave failure; virtualized large collection. |
| **Sequence Viewer** | No sequence; Ready at first/middle/final frame; unreadable sequence; silently skipped missing frame; Dataset/Run sequence source. |
| **Train** | No Dataset; no Labeled crops; CPU fallback; GPU; Starting; Running; Stopping; Completed low metrics; Automatic Save; save failure; Active Model update; Interrupted/Failed. |
| **Model Test** | Missing selections; class mismatch; GPU; CPU fallback; Running; Completed 2-class; Completed 3-class; Interrupted; failed output; Active Model unchanged. |
| **Library** | Empty; Add Model empty/duplicate Name; one Active model; selected nonactive; idle selected Active with Remove available; Active Model empty after confirmed removal; running-consumer locked package; long list; import/rename/remove failure; remove confirmation; retraining source protected/new Name. |
| **Live** | Every state and condition listed in §15.19, including no model Trigger Every Droplet, two/three-class Class-Based, both Hit directions, panel lock, counters, Unresolved, and preservation outcomes. |
| **Sequence Test** | DAQ enabled/disabled; DAQ unavailable; no Camera; with/without model; two/three classes; Running; Completed/Interrupted/Failed. |
| **Runs list** | Empty; mixed statuses and operation types; unreadable entry; no model; long names. |
| **Selected Run** | Completed; Stopped; Interrupted; Failed; full sequence/no sequence; missing log; Unresolved matrix; Notes edit/save failure. |
| **Settings** | Default/custom root; invalid root; version/runtime facts; drivers unavailable; diagnostic folder failure. |
| **Configuration panel** | Every state listed in §19.11. |
| **Setup Profile** | Every state listed in §20.9. |

## 25.6 Mock-data naming and provenance

Mock scenario names SHOULD follow a stable pattern:

```text
<workspace>.<state>.<variant>
```

Examples:

```text
live.running.threeClass
live.interrupted.cameraDisconnectPreserved
sequenceTest.ready.daqDisabledNoHardware
label.ready.largeMixedStates
hardware.locked.datasetCapture
```

Each fixture should include a short note identifying the PM/IA/LF state it represents. It must not use old decision-register identifiers from PDS v0.1.

**Source basis:** *OpenDSS Approved v2 Product Model* workspace, operation, fault, and boundary requirements in §§5–17; *OpenDSS v2 Information Architecture and Screen Inventory* complete workspace/state inventory in §§3–6; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* detailed state catalog and acceptance checklist in §§2–20; *OpenDSS Detailed User Workflow Specification* end-to-end acceptance and nonfunctional requirements in §§31–43 and §§44–72; *OpenDSS Product Design Specification*, Draft v0.1 §§11–12 as adapted mock-data and review evidence.

---

# 26. Design QA and acceptance evidence

## 26.1 Design QA objective

Design approval requires evidence that the specification is represented consistently in the maximized window across scaling factors, applicable states, keyboard paths, accessibility requirements, resource locks, fault/recovery behavior, and Qt Design Studio artifacts.

A screen shown only in Ready/ideal state is incomplete.

## 26.2 Required evidence matrix

| Evidence area | Required evidence | Pass condition |
|---|---|---|
| **Window state** | Reference screenshots or deterministic renders in the maximized window and, where responsive/restored behavior is relevant, at exactly 1600 × 900 logical px or larger. Never validate below 1600 × 900. | No clipping, hidden primary action, or unusable content. |
| **Scaling** | 100%, 125%, 150%, and 200% evidence for shell, components, and representative dense screens. | Text/controls remain legible and operable; no essential clipping; icons/focus remain crisp. |
| **Workspace states** | Every applicable state in §8 and §25. | State, action, disabled reason, lock, banner, and next action match the authoritative interaction baseline. |
| **Keyboard operation** | Recorded keyboard walkthrough or test checklist for every core workflow. | All required actions reachable; shortcuts scoped; typing suppresses workspace keys. |
| **Visible focus** | Screenshots/video or automated visual checks for selected components/screens. | Focus is always visible and distinct from selection/class/state. |
| **Tab order** | Documented focus sequence for shell, each workspace, panel, and dialogs. | Order follows task/visual sequence; dynamic fields do not create traps. |
| **Contrast** | Token-level and rendered-state contrast report. | Meets text and non-text requirements in all relevant combinations. |
| **Target size** | Component measurements and exception review. | At least 24×24 logical px; balanced controls meet preferred sizes. |
| **Non-color cues** | State inventory screenshots in color and grayscale or simulated color-vision conditions. | Every required state remains identifiable without hue alone. |
| **Accessible names** | Accessibility-tree inspection for representative forms and custom controls. | Names, roles, values, selected/checked/expanded/disabled states are correct. |
| **Enlarged text** | 200% scaling plus long text/path fixtures. | No loss of function or essential text. |
| **Virtualized collections** | Label fixture with at least 100,000 mock entries; scroll/selection evidence. | Responsive navigation and bounded item creation; state badges remain correct. |
| **Splitter/collapse** | Pointer and keyboard resizing, collapse, restore, and reset evidence. | No state/selection loss; focus returns correctly. |
| **Hardware locking** | Panel and affected workspace evidence for every owner in §19.6. | Editable/read-only state agrees with operation ownership; Live closes the panel. |
| **Disabled reasons** | Primary-action blocker permutations from LF §16. | Exactly one direct operation-level reason shown in correct priority. |
| **Fault/recovery** | Interrupted/Failed banners with preserved, partial, and no-output variants. | What stopped, reason, preservation, and direct actions are factual and singular. |
| **Qt Design Studio validity** | All `.ui.qml` forms opened and previewed in the supported Qt Design Studio version. | No parse errors, missing tokens, broken bindings, or design-only production controls. |
| **Visual regression** | Approved baseline images for components and screen states. | Deterministic renders; intentional changes reviewed and traceable. |

## 26.3 Workspace acceptance checklist

Each workspace review MUST verify:

1. approved title and navigation path;
2. correct startup/direct-navigation behavior where relevant;
3. required artifact/hardware and direct blocker;
4. one dominant primary action per state;
5. no excluded or duplicate controls;
6. correct operation-side-panel content and transition;
7. correct lock ownership and header projection;
8. correct output artifact and completion actions;
9. correct Empty/Unavailable/Ready/lifecycle/fault presentations;
10. approved terminology, including Class Score and Observed Route = Unresolved;
11. keyboard and focus behavior;
12. supported resizing/minimum-width behavior;
13. accessible names, contrast, target size, and non-color cues;
14. mock-data coverage;
15. source traceability to PM, IA, LF, and WF.

## 26.4 Component acceptance checklist

Every shared component MUST be reviewed for:

- token-only styling rather than screen-local literals;
- all applicable interaction states;
- enabled/disabled/busy behavior;
- keyboard activation;
- focus visibility;
- accessible name/role/value/state;
- light/dark context;
- minimum target size;
- long text and 200% scaling;
- semantic state/class/selection conflicts;
- deterministic mock rendering.

## 26.5 Hardware and operation evidence

Design QA must include state sequences, not isolated screenshots:

- idle panel edit → successful immediate apply;
- invalid edit → direct reason → reversion to last applied value;
- Image Sequence Start → Camera lock while DAQ remains editable;
- Droplet Dataset Capture Pause → Camera remains locked;
- Sequence Test DAQ on → DAQ lock; Camera available;
- Sequence Test DAQ off → no hardware lock;
- Live Start → panel forced closed → Running → Paused → Resume → Stop → panel availability restored;
- another operation active → conflicting Start disabled everywhere;
- Active Model in use → Set Active replacement and package mutation disabled.

## 26.6 Fault and preservation evidence

At minimum, verify:

- Camera unavailable before Start;
- Camera disconnect during Image Sequence, Droplet Dataset Capture, and Live;
- DAQ unavailable before Live/physical-output Sequence Test;
- DAQ fault during Live/Sequence Test;
- Model/Dataset class-count mismatch;
- unsupported v2 schema;
- output becomes unwritable during operation;
- Label atomic save failure;
- Run Notes save failure;
- Training runtime failure;
- Model Package mutation failure.

Each case must show one direct message and correct actions without a notification center or repeated modal chain.

## 26.7 Visual-regression references

Baseline names SHOULD encode:

```text
<workspace-or-component>__<state>__<size>__<scale>.png
```

Examples:

```text
live__running-three-class__maximized__150.png
label__mixed-states-large__maximized__100.png
hardware-panel__camera-locked-sequence__maximized__200.png
```

Baselines must use deterministic mock fixtures and record token/version identifiers. A visual change cannot be accepted solely because pixel difference is small; reviewers must confirm that product state and accessibility remain correct.

## 26.8 Design-review package

The review package SHOULD contain:

- the current Markdown specification;
- change summary from the previous consolidated draft, not from the obsolete PDS v0.1;
- token reference;
- component gallery;
- screen-state gallery;
- responsive/scaling matrix;
- accessibility report;
- keyboard/focus walkthrough;
- lock/fault state sequences;
- Qt Design Studio validity report;
- visual-regression index;
- open design defects and owners.

This is review evidence, not a new product decision register.

**Source basis:** *OpenDSS Approved v2 Product Model* §§13–19; *OpenDSS v2 Information Architecture and Screen Inventory* workspace/state/lock/trace sections 3–8; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§16–22; *OpenDSS Detailed User Workflow Specification* acceptance, persistence, error, nonfunctional, and ownership sections 31–43 and 44–78; *OpenDSS Product Design Specification*, Draft v0.1 §12 as adapted QA and visual-regression evidence.

---
# 27. Requirements and source traceability

## 27.1 Traceability method

This section maps each major design section to the controlling product, IA, interaction/state, workflow, and supporting design-evidence sources. The mapping does not create a new product decision register.

Decision identifiers D-001 through D-019 belong exclusively to the *OpenDSS Approved v2 Product Model*. PDS v0.1 decision identifiers are not reused. This consolidated draft introduces no design-rationale identifier because none is required to explain the adopted visual system.

## 27.2 Major-section trace matrix

| Consolidated design section | Approved Product Model authority | IA baseline | Low-fidelity/state baseline | Detailed Workflow requirements retained | PDS v0.1 design evidence |
|---|---|---|---|---|---|
| **1. Status, purpose, scope, authority** | PM §1, §§19–21 | IA authority statement and §§7–8 | LF authority statement and §§21–23 | WF §§1–4 and §§73–78 | Scope/handoff evidence only; old authority claim superseded |
| **2. Users and principles** | PM §§2–3, §§13–14, §17 | IA §§1–6 | LF §§1–4, §§17–20 | WF §§2, 4–6, 9, 36, 38–43 | §2 principles adapted; role labels treated as task contexts, not permissions |
| **3. Information architecture** | PM §4; D-001, D-006, D-007, D-011, D-013, D-014, D-017, D-018 | IA §2 and workspace inventory | LF §§2.2–2.4 | WF §7 as explicitly amended by PM | Old §3 navigation not carried forward; compact/expanded rail only adapted |
| **4. Application shell** | PM §5, §14; D-016, D-018, D-019 | IA §1 | LF §2, §§4–5 | WF §§8–9, §34 | §4 geometry and §5 visual hierarchy adapted |
| **5. Visual design system** | PM §§3, 5, 10, 14 | IA shell/state requirements | LF structure and state language | WF terminology, errors, usability, transparency | §§1 and 5, plus §§9–10, adapted as visual evidence |
| **6. Shared components** | PM shell/workspace/fault behavior | IA §§1, 3–6 | LF shared patterns and detailed workspace controls | WF workflows and accessibility/nonfunctional requirements | §6 adopted/adapted; excluded controls omitted |
| **7. State language and faults** | PM §§3.2–3.3, 10, 13–14; D-019 | IA §4 state inventory | LF §§3–4, §16, §19 | WF §§4, 8–9, 31–34 | §7 adapted; obsolete `Armed` product state not introduced |
| **8. Workspace framework** | PM §§4–14 | IA §§1–6 | LF §§1–20 | WF §§7–23, 31–43 | Layout/component/accessibility evidence only |
| **9. Capture** | PM §7.1, §§8, 11–14; D-014, D-016 | IA §§3.1, 4.2–4.4, §5 | LF §6, §§16–18 | WF §§11–13 amended to fixed detector/crop configuration | Viewer/component evidence adapted |
| **10. Label** | PM §7.2, §§8, 11, 13–15; D-012, D-019 | IA §§3.1, 4.5, §§5–6 | LF §7, §§16–18 | WF §14, §§31–34 | Thumbnail distinctions, keyboard, responsive evidence adapted |
| **11. Sequence Viewer** | PM §7.3, §8, §12 | IA §§3.1, 4.6, §5 | LF §8, §18 | WF §18 | Dark viewer, controls, keyboard evidence adapted |
| **12. Train** | PM §7.4, §§8, 11–13; D-003, D-012, D-015 | IA §§3.1, 4.7 | LF §9, §§16–17 | WF §15 amended to remove Advanced parameters | Metrics, progress, handoff evidence adapted |
| **13. Model Test** | PM §7.5, §§11–13; D-004, D-011, D-012, D-013 | IA §§3.1, 4.8 | LF §10, §§16–17 | WF §16 amended for automatic GPU/CPU policy | Metrics/table/accessibility evidence adapted; `Validate` terminology rejected |
| **14. Library** | PM §7.6, §8, §13, §16; D-010 | IA §§3.1, 4.9, §6 | LF §11, §§16–18 | WF §17 amended to v2-only package support | Master-detail structure adopted; scientific package states rejected |
| **15. Live** | PM §7.7, §§8–14; D-002, D-005, D-007, D-008, D-009, D-016, D-019 | IA §§3.1–3.2, 4.10–4.13, §§5–6 | LF §12, §§16–19 | WF §§20–21 amended into one Live workspace and Unresolved route | Viewer/metric/component evidence adapted; old Live state model not authoritative |
| **16. Sequence Test** | PM §7.8, §§8, 10–14; D-004, D-005, D-006, D-008 | IA §§3.1, 4.14, §§5–6 | LF §13, §§16–19 | WF §19 amended for Sort placement and Unresolved | Component/accessibility evidence only |
| **17. Results > Runs** | PM §7.9, §§8, 10, 14–17; D-005, D-013 | IA §§3.1, 4.15–4.16, §§5–6 | LF §14, §§16–19 | WF §22 and Run contracts amended for Unresolved | Table/master-detail evidence adapted; charts and broad history rejected |
| **18. Settings** | PM §7.10, §§11–12; D-017 | IA §§1.4, 3.1, 4.17, §8 | LF §15–16 | WF §23 narrowed to Storage, Application Information, Diagnostics | Form treatment adapted; old System groups rejected |
| **19. Configuration panel** | PM §5.2, §§11–13; D-002, D-016 | IA §§1.6–1.7, §6 | LF §5 and ownership rules | WF hardware/concurrency/error requirements as amended | Contextual panel pattern adapted; visible name is Configuration; content is Camera, DAQ, and the explicitly approved `Minimum Size` threshold only |
| **20. Setup Profile** | PM §9, §11; D-009 | IA §3.2, §5, §8 | LF §12.3, §16 and ownership rules | WF §20 and §30 only where compatible with ordinary-file/narrowed content model | File-control visuals only; managed library discarded |
| **21. Contextual links** | PM §§4, 6, 8 | IA §5 | LF §4.8, §18 | WF handoffs in §§12–22 as amended | No authority; compatible link styling only |
| **22. Keyboard/focus** | PM shell/operation/fault boundaries | IA shell and workspace behavior | LF §§2, 4–18 and acceptance | WF §§9, 14, 18, 21, 38–43 | §9 adopted/adapted |
| **23. Responsive/accessibility** | PM product/platform and state requirements | IA shell/workspace minimum behavior | LF detailed layouts/focus | WF §§38–43 | §10 adopted/adapted; values treated as consolidated design requirements/recommendations as labeled |
| **24. Qt Design Studio/handoff** | PM §18 and repository-as-evidence rule | IA §§6–7 | LF §§3.2, 22–23 | WF §§73–78 | §11 adopted/adapted; production architecture remains out of scope |
| **25. Mock data** | PM workspace/state/fault requirements | IA §§3–6 | LF detailed state catalog | WF §§31–43 and §§44–72 | §§11–12 adopted/adapted |
| **26. Design QA** | PM §§13–19 | IA §§3–8 | LF §§16–22 | WF §§31–43 and §§44–78 | §12 adopted/adapted |
| **27. Traceability** | PM complete, including D-001–D-019 | IA §7–8 | LF §22–23 | WF complete nonconflicting coverage | Evidence only, clearly labeled non-authoritative |
| **28. Design-input disposition** | PM controls all conflicts | IA/LF control current downstream design | LF prohibits reopening alternatives | WF fills nonconflicting detail | PDS v0.1 is the input being classified |
| **29. Explicit exclusions** | PM §§16–17 and D-001–D-019 | IA §8 | LF §§1.2, 21 | WF §43 as further narrowed by PM | Obsolete alternatives explicitly not carried forward |

## 27.3 Approved decision trace

| PM decision | Consolidated design implementation |
|---|---|
| **D-001 — Domain primary navigation** | Section 3 exact Data / Models / Sort / Results / Settings hierarchy. |
| **D-002 — No separate software DAQ arming** | Sections 15, 16, and 19 use DAQ readiness and explicit Physical DAQ Output without an arming workflow. |
| **D-003 — Saved model automatically becomes Active** | Section 12 save-completion state and Section 14 Active Model distinction. |
| **D-004 — Model Test automatic optional GPU, CPU fallback** | Section 13 factual device status; GPU absence never blocks Start. |
| **D-005 — Observed Route adds Unresolved** | Sections 7, 15–17 and all required counters/matrices. |
| **D-006 — Sequence Test under Sort** | Sections 3 and 16. |
| **D-007 — One Live workspace** | Section 15 pre-run through post-operation state transitions; no Sort Setup. |
| **D-008 — Trigger Every Droplet first-class** | Sections 15 and 16 conditional model/Decision behavior. |
| **D-009 — Setup Profiles ordinary files** | Sections 15 and 20 Open/Save/Save As only. |
| **D-010 — No product-level legacy support** | Sections 6–20 use supported-v2 errors; Section 29 excludes migration UI. |
| **D-011 — Model Test first-class** | Sections 3 and 13. |
| **D-012 — Two and three classes** | Label, Train, Model Test, Library, Live, Sequence Test, Results, and class-token system. |
| **D-013 — Results contains Runs only** | Section 17. |
| **D-014 — Shared Capture composition** | Section 9 shared preview and three fixed, independently collapsible section headings with no default expansion. |
| **D-015 — No Advanced Training Parameters** | Section 12 fixed-config presentation and Section 29 exclusion. |
| **D-016 — Camera/DAQ settings plus approved detector exception** | Sections 4 and 19 Configuration behavior. Camera/DAQ ownership rules remain; `Minimum Size` is the sole approved detector-setting exception and remains editable during active Runs. |
| **D-017 — Reduced Settings** | Section 18. |
| **D-018 — Fixed startup** | Sections 3–4: Data > Capture with all three Capture sections collapsed, no last-workspace restore. |
| **D-019 — Simple contextual faults** | Sections 6–7 and every workspace's direct reason/banner contract. |

## 27.4 Conflict disposition rules applied

The following higher-authority corrections are embedded in the consolidated design and are not open alternatives:

| Lower-authority or historical idea | Controlling disposition |
|---|---|
| Sequence Test under Models | PM D-006 and current IA/LF place it under Sort. |
| Separate Sort Setup | PM D-007 and current IA/LF make setup the pre-run state of Live. |
| Observed Route only Hit/Waste | PM D-005 adds Unresolved throughout UI and artifacts. |
| User-editable detector, crop, routing, timing, or training parameters | PM D-015/D-016 and §11 make them fixed qualified configuration. |
| Managed Setup Profile list with Import/Export/Delete | PM D-009 limits product actions to Open, Save, Save As on ordinary files. |
| Results containing Training/Model Test history | PM D-013 limits Results to Live and Sequence Test Runs. |
| Camera/DAQ under Settings/System | PM §5 and D-016 move technical settings to the shell-owned panel; Settings is reduced. |
| Manual CPU/GPU selection | PM §§7.4–7.5 and §11 require automatic device policy. |
| Legacy compatibility and migration UI | PM D-010 and §16 exclude it from the product. |
| Old PDS navigation and decision register | PM and current IA/LF supersede them; old IDs are not reused. |

## 27.5 Requirement classification

This specification distinguishes:

- **product/interaction requirements** — grounded in PM, IA, LF, or nonconflicting WF and expressed with MUST/MUST NOT;
- **consolidated visual recommendations** — inferred from compatible evidence and expressed with SHOULD/SHOULD NOT;
- **optional presentation choices** — expressed with MAY and constrained not to change behavior.

A visual recommendation cannot be implemented in a way that changes an approved workflow, state owner, prerequisite, term, artifact, or operation result.

**Source basis:** *OpenDSS Approved v2 Product Model* (all applicable sections); *OpenDSS v2 Information Architecture and Screen Inventory* (all applicable sections); *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* (all applicable sections); *OpenDSS Detailed User Workflow Specification* (nonconflicting sections); *OpenDSS Product Design Specification*, Draft v0.1 (supporting design evidence only). The repository is optional implementation evidence only.

---

# 28. Design-input disposition appendix

## 28.1 Purpose

This appendix classifies significant ideas from *OpenDSS Product Design Specification*, Draft v0.1, according to the controlling PM, IA, LF, and nonconflicting WF requirements. `Adopted` means the design idea carries forward substantially intact. `Adapted` means its useful design intent is retained but changed to fit the approved v2 product. `Not carried forward` means it conflicts with or is unnecessary under the controlling sources.

The appendix is not an alternative-design menu. Rejected ideas do not remain available options in the main specification.

## 28.2 Disposition table

| PDS v0.1 idea | Disposition | Consolidated treatment and reason |
|---|---|---|
| **Droplet-and-branch visual identity** | **Adopted** | Retained as a restrained brand concept because it does not change product behavior. A dedicated small mark is required rather than mechanically shrinking detailed artwork. |
| **Navy, blue, teal, and neutral palette direction** | **Adapted** | Retained as the palette direction, with semantic roles separated and contrast validation required. Exact values are review tokens, not inherited mandates. |
| **Dark live-view canvas** | **Adopted** | Used for Camera, Droplet Crop, and Sequence viewing while the application shell remains light. |
| **Typography direction** | **Adapted** | Legible sans-serif using the approved Medium (100%) typography ramp: 16 px / 20 px body and standard controls, 16 px / 18–20 px buttons, 15 px / 18 px ordinary labels, 13 px / 16–18 px captions, status, warning, and metadata, and 22–32 px metrics. Tabular figures and limited monospace remain. Typeface choice remains subject to bundling/rendering validation. |
| **Spacing and geometry scale** | **Adapted** | 4-based spacing, balanced 36 px controls, restrained radii, and flat surfaces retained as recommendations; dimensions are adjusted to the approved shell and accessibility needs. |
| **Compact and expanded navigation presentation** | **Adapted** | Permitted for the approved Data/Models/Sort/Results/Settings rail. The old navigation labels and hierarchy are not retained. |
| **One summarized system-status control** | **Adapted** | The intent to avoid redundant status clutter is retained, but the master requires four explicit header areas: Camera, DAQ, Active Model, Current Activity. Configuration detail uses the bottom Configuration panel rather than one opaque summary control. |
| **Shared component system** | **Adopted** | Expanded in Section 6 and constrained to approved features/states. One-off screen styles remain defects. |
| **Dataset thumbnail state distinctions** | **Adopted** | Selection, focus, class identity, Labeled, Skipped, and Removed are explicitly separated and made non-color dependent. Old review/exclusion semantics are translated to approved crop states. |
| **Master-detail Model Library** | **Adopted** | Fits the approved Library actions and selected-vs-Active distinction. Old valid/approved/candidate-style states are omitted. |
| **Contextual panels** | **Adapted** | Retained specifically as the shell-owned Camera/DAQ panel. Detector, crop, model, or generic System panels are not added. |
| **Keyboard workflows** | **Adopted** | High-throughput Label and viewer shortcuts, focus visibility, and typing suppression are incorporated and expanded. Hardware operation shortcuts remain deliberately constrained. |
| **Accessibility requirements** | **Adopted** | Contrast, non-text contrast, targets, non-color cues, accessible names, keyboard operation, and enlarged text are required. |
| **Responsive and high-DPI requirements** | **Adopted** | Maximized full-available-work-area validation and 100/125/150/200% scaling are consolidated. Restored and fixed-resolution checks are removed; workspace structure remains fixed. |
| **Qt Design Studio authoring** | **Adopted** | Editable `.ui.qml` forms, wrappers, view-model projections, tokens, components, mocks, and galleries are retained within a design-handoff boundary. |
| **Mock data** | **Adopted** | Required for every component and workspace state, including fault and locking combinations. |
| **Visual-regression evidence** | **Adopted** | Deterministic component/screen baselines are part of design QA. |
| **Old primary navigation: Live / Data / Models / Runs / System** | **Not carried forward** | Conflicts with PM D-001 and the approved IA. The required hierarchy is Data / Models / Sort / Results / Settings. |
| **Capture inside Live** | **Not carried forward** | Conflicts with PM D-014. Capture is one Data workspace with three equal modes; Live is physical sorting. |
| **`Validate` terminology for Model Test** | **Not carried forward** | The approved workspace and scientific term is Model Test. `Validate` can imply approval and conflicts with current terminology. |
| **Camera and DAQ under System** | **Not carried forward** | PM D-016 requires a shared shell-level Camera/DAQ panel. Settings contains Storage, Application Information, and Diagnostics only. |
| **Editable detector controls** | **Not carried forward** | Conflicts with PM §11 and approved constraints. Detection is fixed qualified application configuration and recorded in provenance. |
| **Advanced Training Parameters** | **Not carried forward** | Training consumes Library-owned identity, Architecture, and Starting Weights read-only and exposes only the approved Dataset, Compute Device, Output Location, and fixed configuration facts. |
| **Old decision-register identifiers D-001, D-002, etc.** | **Not carried forward** | Those identifiers now belong exclusively to the Approved Product Model. This specification does not reuse them or create a new product decision register. |
| **One generic System-status panel containing all settings** | **Not carried forward** | Configuration contains Camera, DAQ, and only the approved `Minimum Size` threshold; Storage/Application Information/Diagnostics remain in Settings. |
| **`Armed` as a product-level operational state** | **Not carried forward** | PM D-002 adds no separate software arming state. DAQ readiness and explicit Physical DAQ Output provide factual technical status. |
| **Camera settings opened contextually from Live** | **Adapted** | Retained through global Configuration, available from all workspaces while idle rather than owned by Live. |
| **Resizable/collapsible side panes** | **Adopted** | Retained with keyboard alternatives, stable selection/focus, and minimum-width rules. Active operation panels cannot disappear automatically. |
| **Five persistent equal-weight status chips replaced by concise status** | **Adapted** | Redundant chip styling is rejected, but the four approved labeled status areas remain continuously visible and are not collapsed into one ambiguous sentence. |
| **Misclassified-image review in Model Test** | **Not carried forward** | The first release requires metrics, confusion matrix, summary, and predictions CSV; no integrated misclassified-image browser is added. |
| **Broad searchable Runs history with exports/logs as separate areas** | **Adapted** | Runs list and direct files are retained, but Results is limited to Live/Sequence Test Runs and has no separate Exports/Logs navigation or first-class charts. |
| **Hardware-safe primary action logic** | **Adapted** | The useful intent is retained through exact technical prerequisites, direct disabled reasons, resource ownership, and fault stop; obsolete setup/readiness proposals are replaced by current LF behavior. |

## 28.3 Disposition summary

The carried-forward design value of PDS v0.1 is its restrained visual identity, component discipline, thumbnail-state separation, master-detail pattern, keyboard/accessibility emphasis, responsive/high-DPI matrix, Qt Design Studio authoring model, mocks, and QA evidence.

Its product structure, navigation, terminology, editable-parameter proposals, status model, and decision identifiers are not authoritative and are not preserved where they conflict with the approved v2 model.

**Source basis:** *OpenDSS Product Design Specification*, Draft v0.1, all sections; disposition controlled by *OpenDSS Approved v2 Product Model* §§1–21, *OpenDSS v2 Information Architecture and Screen Inventory* §§1–8, *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§1–23, and nonconflicting *OpenDSS Detailed User Workflow Specification* requirements.

---

# 29. Explicit exclusions

## 29.1 Excluded product and navigation elements

The consolidated OpenDSS v2 design MUST NOT include or imply:

- a Home screen or dashboard;
- a separate Sort Setup workspace;
- a separate Reports workspace;
- Training history under Results;
- Model Test history under Results;
- repository modules or source directories as navigation categories;
- alternate primary navigation proposals;
- a mandatory wizard, project flow, or numbered workflow progress system;
- placeholder controls or navigation items for deferred features.

## 29.2 Excluded editable configuration

The normal product MUST NOT expose:

- editable droplet-detection settings;
- editable Droplet Crop settings beyond the fixed artifact contract;
- editable routing-algorithm settings;
- editable internal tracking, synchronization, or timing settings;
- editable training hyperparameters;
- Advanced Training Parameters;
- manual CPU/GPU selection;
- confidence- or Class Score-threshold routing;
- raw JSON editing for normal workflows;
- a software DAQ arming state.

Camera and DAQ technical settings plus the approved `Minimum Size` minimum contour-area threshold are the only user-editable technical settings and exist only in shared Configuration.

## 29.3 Excluded scientific policy and status

The design MUST NOT add:

- scientific quality gates;
- Dataset quality scores;
- class-balance warnings;
- model suitability recommendations;
- approval, candidate, rejected, promoted, archived, certified, or validated model states;
- automatic rejection because of low accuracy, model collapse, one dominant class, or unusual experimental choices;
- mandatory Model Test;
- `Confidence` as a substitute for Class Score;
- `Actual Destination`, `Ground Truth Route`, or `Routing Accuracy` as a substitute for Observed Route or the Decision-versus-Observed Route matrix.

## 29.4 Excluded identity, collaboration, and network features

The design MUST NOT include:

- projects or project hierarchy;
- accounts, authentication, permissions, or role administration;
- cloud workspaces, cloud storage, cloud training, or automatic upload;
- collaborative labeling or collaboration services;
- telemetry;
- update checks or automatic updates;
- remote control;
- network-dependent normal workflows.

## 29.5 Excluded Setup Profile and legacy workflows

The design MUST NOT include:

- a managed Setup Profile library;
- Setup Profile Import, Export, Delete, archive, promotion, or migration actions;
- product-level legacy loaders;
- automatic legacy conversion;
- migration screens;
- read-only legacy mode;
- compatibility branches presented to the user;
- partial interpretation of unsupported schemas.

Supported v2 Setup Profile actions are Open, Save, and Save As only.

## 29.6 Excluded hardware and safety claims

The design MUST NOT include:

- a named No-Camera Mode, Diagnostic Mode, or no-hardware mode;
- a GUI control labeled or described as a safety-rated Emergency Stop;
- wording that implies normal Stop or fault handling replaces physical safety controls;
- multiple simultaneous Camera or sort-path controls in the first release;
- hardware settings duplicated inside Capture, Live, Sequence Test, or Settings.

## 29.7 Excluded Dataset, model, and Results expansions

The first-release design MUST NOT add:

- more than three classes;
- arbitrary user-supplied model architectures;
- external arbitrary image import into Datasets;
- Dataset merging;
- user-facing Dataset versioning, branching, or approval;
- an integrated per-event Run browser;
- first-class Run charts;
- an integrated misclassified-image browser;
- Training or Model Test output represented as a Run;
- automatic scientific interpretation of metrics or matrices.

## 29.8 Exclusion enforcement in design files

Excluded features must not appear as:

- disabled placeholder buttons;
- hidden tabs waiting for implementation;
- mock-only navigation items;
- commented alternate forms presented for review;
- component-gallery variants implying future product behavior;
- repository-derived controls that are not approved.

Future features require separate product approval and a corresponding revision of the controlling product model and downstream specifications.

**Source basis:** *OpenDSS Approved v2 Product Model* §§11, 16–17 and D-001 through D-019; *OpenDSS v2 Information Architecture and Screen Inventory* §8; *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification* §§1.2 and 21; *OpenDSS Detailed User Workflow Specification* §43 as further narrowed by *OpenDSS Approved v2 Product Model*; *OpenDSS Product Design Specification*, Draft v0.1 alternatives classified in Section 28.

---

# Source register and citation index

## A. Controlling source: Approved Product Model

**Document:** *Open Droplet Sorting Suite (OpenDSS) — Approved v2 Product Model*  
**Document ID:** ODSS-PM-002  
**Version:** 1.0  
**Source status:** Approved product model  
**Date:** July 21, 2026

Principal coverage used in this specification:

- §1 — purpose and authority;
- §§2–3 — local single-user product definition, scientific authority, factual terminology, ordinary files, reproducibility;
- §4 — approved navigation, no Home, fixed startup, contextual links;
- §5 — global header and bottom Configuration panel;
- §6 — goal-to-workspace map;
- §7 — complete approved workspace model;
- §§8–10 — artifact, Setup Profile, and scientific event models;
- §11 — user-editable versus fixed configuration;
- §§12–14 — dependencies, operation lifecycle, faults, and recovery;
- §§15–18 — persistence, v2-only boundary, first-release exclusions, and state ownership;
- §19 — approved decisions D-001 through D-019;
- §§20–21 — required amendments and next design phase.

This source supplied controlling product-decision provenance during consolidation. `ODSS-DES-002` is now the master authority; this incorporated source cannot independently override it.

## B. Current downstream baseline: Information Architecture

**Document:** *OpenDSS v2 Information Architecture and Screen Inventory*  
**Source status:** Consolidated OpenDSS v2 design baseline

Principal coverage used:

- §1 — shell, header, navigation, workspace region, operation-side panel, panel, startup, shell diagram;
- §2 — exact navigation hierarchy and item purposes;
- §3 — complete workspace inventory, sorting semantics, and Profile behavior;
- §4 — complete state inventory by workspace;
- §5 — contextual workflow links;
- §6 — resource ownership and lock matrix;
- §7 — workflow and approved-decision trace;
- §8 — removed/hidden controls and terminology boundaries.

## C. Current downstream baseline: Interaction and application state

**Document:** *OpenDSS v2 Low-Fidelity Interaction and Application-State Specification*  
**Source status:** Interaction and state-definition baseline

Principal coverage used:

- §1 — interaction objectives, constraints, notation, and axioms;
- §2 — shell interaction, startup, in-session retention, header projection, fault placement;
- §3 — conceptual state model, authoritative owners, lifecycle, snapshots;
- §4 — shared interaction patterns, disabled reasons, Start/Stop/Pause, banners, validation;
- §5 — hardware panel behavior;
- §§6–15 — detailed workspace layouts and interactions;
- §16 — primary-action enablement and reason priority;
- §17 — shared resource ownership and navigation effects;
- §18 — contextual handoffs;
- §19 — fault and recovery catalog;
- §§20–21 — acceptance checklist and explicitly absent interaction elements;
- §§22–23 — source trace and handoff boundary.

## D. Detailed nonconflicting workflow source

**Document:** *Open Droplet Sorting Suite (OpenDSS) — Detailed User Workflow Specification*  
**Document ID:** ODSS-WF-001  
**Version:** 1.0  
**Source status:** Product workflow baseline  
**Date:** July 21, 2026

Principal nonconflicting coverage used:

- §§1–6 — purpose, product objective, principles, user/platform, terminology;
- §§8–10 — global status, operation/concurrency behavior, and default storage;
- §§11–14 — Single Image, Image Sequence, Droplet Dataset Capture, Label;
- §§15–17 — Train, Model Test, Model Library, subject to PM amendments;
- §§18–19 — Sequence Viewer and Sequence Test, subject to Sort placement and Unresolved;
- §§20–22 — run setup, Live Sorting, Results, consolidated and amended by PM/IA/LF;
- §§24–30 — canonical artifact/file contracts, subject to v2 amendments;
- §§31–34 — persistence, atomic writes, crash recovery, and errors;
- §§35–43 — installation/first launch, camera-free use, provenance, nonfunctional requirements, and exclusions;
- §§44–72 — end-to-end acceptance scenarios, interpreted through the approved product model;
- §§73–78 — authoritative service concepts, tests, repository alignment, engineering discretion.

Conflicting historical requirements—such as old navigation, separate Sort Setup, editable detector/crop/training controls, managed Profiles, manual device selection, and two-value Observed Route—are not used.

## E. Supporting design evidence only

**Document:** *OpenDSS Product Design Specification*  
**Source status:** Draft v0.1  
**Date:** July 21, 2026

Compatible evidence used:

- §1 — droplet-and-branch identity, navy/blue/teal direction, light surfaces, dark viewer, geometric icons, state separation;
- §2 — state clarity, local context, recovery, keyboard repetition, restrained brand;
- §§4–7 — shell geometry suggestions, visual tokens, component patterns, state/message styling;
- §8 — selected useful workspace patterns only, such as master-detail Library and virtualized Dataset review;
- §§9–10 — keyboard, responsive, high-DPI, and accessibility evidence;
- §§11–12 — Qt Design Studio authoring, mock data, component gallery, QA, and visual regression.

Its navigation, product behavior, editable controls, old terminology, open alternatives, and old D-identifiers are not authoritative. Section 28 records their disposition.

## F. Optional implementation and historical evidence

**Repository:** `https://github.com/haeminjung12/OpenDSS_clean`

Evidence reviewed at a high level includes:

- C++/Qt/CMake desktop application structure;
- `app/runtime` hardware, detection, metadata, sequence, persistence, and ONNX components;
- `app/runtime/desktop_app` application resources, styles, widgets, controllers, and existing screens;
- `training/python` bundled training package structure;
- branding source/export assets and historical screenshots.

The repository may supply reusable implementation assets after technical review. It does not define the approved navigation, terminology, editable settings, state model, or product scope.

## G. Consolidated authority statement

This consolidated document is the controlling master specification. PM, IA, LF, WF, PDS v0.1, amendments, reviews, and repository evidence listed in this appendix are provenance inputs already consolidated here; they are not separate authorities over this document.

1. Implement and validate the requirements in this document.
2. Use cited source documents only to understand provenance or a reference explicitly incorporated by this document.
3. Do not allow a derived plan, ledger, review, existing implementation, or provisional fallback to override this document.
4. If a conflict or ambiguity remains within this document, stop the affected work and ask the user to clarify.
5. Change this authority only through an explicit user-approved amendment to this document and its lock record.
