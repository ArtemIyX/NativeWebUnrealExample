# NativeWeb asset-backed apps

## Decisions and scope

- Preserve existing file-based and inline view APIs. Add a web app primary data asset with an editable nonempty `FName` ID, a fixed C++ primary asset type, Blueprint access to its primary asset ID, and duplicate ID validation. Future lookup by type and ID is anticipated; launch by ID is out of scope.
- Reference HTML, CSS, JavaScript, and font assets with soft object pointers. HTML supports Asset, multiline String, and Default (the existing shell containing `<div id="app">`). CSS and JavaScript support Asset, String, and None. Show only fields relevant to the selected mode.
- JavaScript Evaluate on Start defaults to false. Retain the selected script for a dedicated view API to evaluate later.
- Provide HTML, CSS, JavaScript, and font data assets storing imported bytes. Text assets show read-only imported size in KB. Create them empty and provide editor Import, Export, and Clear buttons. Accept matching file types and TTF/OTF fonts; retain font filenames for `fonts/<filename>` URLs and reject collisions within an app.
- Export exact imported bytes. Import and Clear support undo and mark packages dirty; cancellation or failure preserves content. Decode text as UTF-8 at launch. Keep dialogs and authoring support in an editor-only module and runtime assets/loading in the runtime module.
- Add synchronous and asynchronous asset-backed local-player subsystem launch APIs. Async accepts a soft app reference, loads selected dependencies, reports success or structured failure to C++ and a Blueprint async node, manages load lifetime and teardown cancellation, and prevents duplicate in-flight view names.
- Selected missing, invalid, or unloadable assets fail with source-specific errors. Startup JavaScript failure closes and unregisters the partial view.
- Document Asset Manager registration and cook rules for apps and soft dependencies and provide a packaged example.

## Acceptance and verification

- Cover source modes, UTF-8, delayed evaluation, script failures, missing assets, duplicate names, async teardown, and legacy launch behavior.
- Cover editor import/export byte round trips, cancellation, Clear, size, font names and collisions, undo, and package dirty state.
- Build the editor and a packaged target. Cook and launch an app with soft-referenced assets without loose HTML, CSS, JavaScript, or font files.
- Run applicable NativeWeb automation on D3D11 and D3D12. Report exact results, limitations, and unresolved findings.
