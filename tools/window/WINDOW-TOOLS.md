# Windowed desktop tools

Work in progress: replace the launcher routes for text editing, settings and
bitmap viewing/editing with cooperative guest-side window applications. Reuse
the file manager and DOS file APIs; preserve existing standalone EXEs. Each
window must coexist with the live DOS scheduler, move/resize/minimize, and protect
unsaved work. Tests must inspect real saved files and background CPU progress.
