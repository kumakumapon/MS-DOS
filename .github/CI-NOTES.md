# Native filer browser validation

The native CI verifier must wait for the guest to acknowledge each selection
before sending the next arrow. A blind loop can overrun repainting and miss
SOURCE.TXT. Synchronize the existing WebNP2 verifier's directory-aware selection,
completed file-write checks, and pane-navigation acknowledgements. Validate using
normal guest keys and real FAT12 reads. Keep this follow-up limited to CI code.
