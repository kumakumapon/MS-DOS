# KDESK rendering snapshot contract

The optional 64-byte KDPERF01 producer exposes ready and a nested busy counter.
A debugger may read composed text/attributes only while ready is one and busy
is zero. The counter must cover transient text writes as well as visible pixel
composition. Full scenes and clock updates already use it; incremental DOS row
updates, partial scene updates and resource updates need the same coverage.

The planned repair keeps the record version, layout and drawing behavior. It
will cover mutation through restoration/presentation, and test unchanged Memo
characters, attributes and pixels over continuing background DOS output on
PEGC and Cirrus. The original DOS kernel and saved disks are unaffected.

Implementation and verification results will be recorded here before review.
