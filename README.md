# validate_smalltalk_image

Standalone validator for Spur-format Smalltalk image files. Walks the entire
heap and checks structural integrity, class table consistency, method validity,
and more. Reports errors (fatal corruption) and warnings (suspicious but
non-fatal conditions).

## Supported Image Formats

    Format    Description                  Dialect
    68021     Spur 64-bit                  Pharo 5+, Squeak 5.3+, Cuis 6+
    68533     Spur 64-bit + Sista V1       Pharo 9+ (Sista bytecodes)
    6521      Spur 32-bit                  Squeak 5.x (32-bit), Cuis 5.x
    6505      Spur 32-bit + Sista V1       (if used)

Also supports iOS-variant tag encoding used by [iospharo](https://github.com/avwohl/iospharo)
via the `--ios-tags` flag.

## Building

Requires CMake 3.16+ and a C++17 compiler.

    mkdir build && cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release
    cmake --build .

## Usage

    validate_smalltalk_image [options] <image-file>

    Options:
      --ios-tags       Use iOS tag variant (for iospharo-saved images)
      --reachability   Run reachability analysis (finds unreachable objects)
      --json           Output JSON instead of human-readable text
      --verbose        Print findings as they are discovered
      --max-errors N   Stop after N errors (default: unlimited)
      --help           Show help

    Exit codes:
      0  No errors (warnings are OK)
      1  Errors found (image has structural problems)
      2  Usage error

## Validation Checks

### Errors (structural corruption, exit code 1)

    Check                         Description
    heap.objectOverflow           Object extends past end of heap
    alignment.misaligned          Object header not 8-byte aligned
    classIndex.outOfRange         Class index exceeds 22-bit maximum
    forwarded.inSavedImage        Forwarded object (classIndex=8) found in saved image
    oop.invalidPointer            Pointer slot contains invalid Oop (not immediate, not in heap)
    oop.misalignedPointer         Pointer slot contains unaligned heap address
    oop.notObjectStart            Pointer targets mid-object, not a header
    classTable.missingClass       Object references class index not in class table
    classTable.badPage            Class table page pointer invalid
    classHierarchy.nonPointer     Class object has non-pointer format
    classHierarchy.tooFew         Class has fewer than 3 slots
    classHierarchy.superCycle     Cycle detected in superclass chain
    classHierarchy.badSuper       Superclass is not a valid Oop
    classHierarchy.badMethodDict  Method dictionary is not a valid pointer
    specialObjects.outOfRange     Special objects Oop outside heap
    specialObjects.notStart       Special objects Oop not an object start
    specialObjects.badFormat      Special objects array is not pointer format
    specialObjects.tooSmall       Special objects array has < 60 entries
    specialObjects.badEntry       Critical special object is not a heap pointer
    method.badHeader              Method header (slot 0) is not a SmallInteger
    method.literalOverflow        numLiterals exceeds available slots
    method.literalPastBody        Literal frame extends past method body
    slot.pastHeap                 Object slot data extends past heap end

### Warnings (suspicious but valid)

    Check                         Description
    format.reserved               Object uses reserved format (6, 7, or 8)
    format.zeroWithSlots          ZeroSized format but has > 0 slots
    format.bytePadding            Byte object with 0 slots but non-zero unused count
    freeChunk.nonZeroFormat       Free chunk (classIndex=0) with non-zero format
    overflow.smallCount           Overflow header with slot count < 255
    overflow.veryLarge            Object larger than 100 MB
    classTable.unexpectedFormat   hiddenRootsObj has unexpected format
    classHierarchy.deepChain      Superclass chain depth >= 200
    classHierarchy.badInstSpec    Instance specification is not a SmallInteger
    classHierarchy.badName        Class name is not a byte or IndexableFixed object
    classHierarchy.mdBadFormat    Method dictionary has non-pointer format
    classHierarchy.mdTooSmall     Method dictionary has < 2 slots
    method.argsVsTemps            numArgs > numTemps in method header
    method.badPrimBytecode        hasPrimitive set but first byte is not 248
    method.primOutOfRange         Primitive index > 8191
    header.veryLargeImage         Image > 4 GB
    image.truncated               File is shorter than header claims
    reachability.unreachable      Objects not reachable from roots (with --reachability)
    specialObjects.nilMismatch    nil slot doesn't point to first heap object
    specialObjects.falseMismatch  false slot doesn't point to second heap object
    specialObjects.trueMismatch   true slot doesn't point to third heap object

### Statistics (always reported)

    Statistic                     Description
    Total objects                 Count of non-free heap objects
    Total size                    Sum of all non-free object sizes
    Free chunks / bytes           Count and size of free space
    Heap utilization              Percentage of heap that is live objects
    Pointer/Byte/Word objects     Breakdown by format category
    Compiled methods              Count of CompiledMethod objects
    Weak objects                  Count of weak/ephemeron objects
    Overflow headers              Objects with > 254 slots
    Pinned / Immutable            Count of flagged objects
    Max slot count / object size  Largest object in the heap
    Class table entries           Number of classes in the class table
    Top 20 classes                Most-instantiated classes by count
    Reachable / unreachable       Object reachability (with --reachability)

## How It Works

The validator loads the image file and processes it in phases:

 1. Parse and validate the Spur image header
 2. Linear heap walk: scan every object from start to end
 3. Per-object structural checks: format, alignment, overflow headers
 4. Pointer slot validation: every Oop in every pointer object is checked
    against the set of known object starts
 5. Class table: validate hiddenRootsObj pages and cross-reference all
    objects' class indices
 6. Special objects array: check critical entries (nil, false, true,
    SmallInteger class, etc.)
 7. Class hierarchy: superclass chains (cycle detection), method
    dictionaries, instance specifications, class names
 8. Compiled methods: header decoding, literal frames, primitive indices
 9. Reachability (optional): BFS from roots to find unreachable objects

The validator works on raw file offsets — it does NOT relocate pointers. All
Oop validation uses the original `startOfMemory` base address as stored in the
image header.

## Spur Object Header Layout

All Spur variants (32-bit and 64-bit) use a 64-bit object header:

    Bits 0-21:   Class index (22 bits, index into class table)
    Bit 23:      Immutable flag
    Bits 24-28:  Object format (5 bits, 0-31)
    Bit 29:      Remembered flag
    Bit 30:      Pinned flag
    Bit 31:      Grey flag (GC)
    Bits 32-53:  Identity hash (22 bits)
    Bit 55:      Marked flag (GC)
    Bits 56-63:  Slot count (0-254, or 255 = overflow header)

Objects with > 254 slots have a two-word header: the preceding 64-bit word
contains the actual slot count in its low 56 bits.

## Performance

On a 52 MB Pharo 13 image (740K objects):

    Without reachability:  ~0.3s
    With reachability:     ~0.9s

## License

MIT License. See [LICENSE](LICENSE).
