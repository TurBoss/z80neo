# The Radiant Crucible (v2.0.0)
## *An Alchemical Press Release & Practitioner’s Talisman for the Awakening of Regulus Forth*

```text
       ▲             ======================================================
      / \            REGULUS FORTH: THE RADIANT CRUCIBLE (v2.0.0)
     /   \           A Self-Hosting, Self-Reproducing Crystalline Organism
    /  ▲  \          for 8-Bit Z80 Silicon, CP/M 2.2 & Multi-Tasking MP/M II
   /  / \  \         ======================================================
  /  /   \  \        "Play is love; attention is sustenance; the garden remembers."
 /__/_____\__\       Mage's Guild Psychonautics · Released September 14, 2026
```

**Binary Artifact:** `REGULUS.COM` (Exactly 45,314 bytes / **44.3KB** ($44.3\text{ KB}$))  
**SHA-256 Seal:** `c60aea1111deb5492ae91ce0d0b018db6b3b756c70a5b2c74745f26f2cc3471c`  
**Target Silicons:** Z80 / Z180 Microcomputers (RCBus, SC720, SC792, SC100, SC126, RomWBW, SCM, CP/M 2.2, MP/M II V2.1)  
**Distribution:** Open under the MIT License for retro-builders, solderers, and stack mystics worldwide.

---

## I. The Invocation: Sovereign Compute on Physical Copper

To the wire-wrappers, the iron-wielders, the night-shift kernel hackers, and the seekers of discoverable compute: **Regulus Forth 2.0.0 has awakened on 8-bit copper.**

Weighing in at **44.3KB** ($44.3\text{ KB}$ / 45,314 bytes), `REGULUS.COM` (v2.0.0) is not merely a language interpreter. It is a **complete, self-hosting, self-reproducing, self-documenting computational organism**. It launches cleanly on vintage and modern Z80 silicon, transforming any bare CP/M 2.2 or MP/M II terminal into an unshakeable, sovereign development laboratory.

There are no external toolchains to install. No host cross-compilers required. No missing paper manuals. The binary is the forge, the library, the compiler, and the seed.

```text
A>REGULUS

=== MAGE'S GUILD PSYCHONAUTICS ===
Play is love; attention is sustenance; the garden remembers.
Type MAN for manual. Type WORDS for dictionary.

ok 
```

---

## II. Four Esoteric Hooks for Curious Minds

Why build a new Forth in 2026? Because the computing world took a 50-year detour through memory bloat and opaque abstraction layers. Regulus returns to the bedrock of reality with four foundational insights:

### 1. The Cellular Organ Architecture & The Holographic Loom
Traditional Forth welded the dictionary, line buffer, editor, compiler, and OS together into an inseparable monolith. Regulus factors computing into **Cellular Organs**:
* Code compiles into dense, portable **Crystal Bytecode** (`.CRY`).
* A single word definition is an organ. It can live in the interactive REPL, be driven by the standalone micro-runner (`VM.COM`), or be packaged into an autonomous `.COM` executable carrying its own embedded native Z80 micro-VM.
* By treating 2D terminal screens as boundary horizons and memory as folded strands, Regulus executes topological stack geometry without floating-point bloat.

### 2. The Inverted-T Centroid Trap (`STILL_POINT`) & Biological Forth
Nature does not use registers or call-stack frame pointers; ribosome translation of mRNA into protein chains is an exact physical implementation of a **dual-stack Forth virtual machine**. 
* Regulus embeds the **Inverted-T Centroid Trap ($\text{outdegree} = 0$)**: zero-entropy halting (`STILL_POINT`).
* Every allocation in the dynamic `HERE` arena enforces $\text{T\_STOP} - \text{DP} \ge \text{size}$. If memory exhausts, execution halts atomically into ground-state equilibrium without clobbering high-RAM operating system buffers or corrupting return stacks.

### 3. The Andean Photonic TRNG & RGM2 Khipu Compression
Hardware randomness and ancient fiber data structures are built into the heart of the machine:
* The resident words **`RANDOM`**, **`CHOOSE`**, **`ROLL`**, and **`ALCHEMY`** implement an in-silico simulation of optical beam splitters, ergodic Fibonacci KAM phase strides, topological S/Z Berry phase kicks, and palindromic self-normalizing whitening.
* The embedded **RGM2 Manual Engine** applies Andean Khipu-inspired schema factoring and a zero-branch table-driven string palette: 88 frequent schema tokens and stack patterns are stored in an archive-embedded dictionary table, page offsets and lengths use compact 16-bit records, and the archive carries a SHA-256 integrity root. It stores **113 complete resident manual pages (100% dictionary coverage)** in a compact archive, streaming directly to console with zero dynamic heap allocations.

```forth
ok 6 ROLL .
4 
ok ALCHEMY
The Stone is not a mineral; it is a lens. Touch the stack -> pure gold.
ok 
```

### 4. Khipu Flow-Control & Spatial Coordinate Navigation
In Regulus v2.0.0, control flow is not a chaotic jump across flat memory—it is **spatial navigation across fiber and knot topologies**:
* **The Bounded Cord Cluster Sweep (`DO ... LOOP` and `I`):** 
  In an archaeological Khipu (such as UR022 or UR006), cords are attached in discrete, bounded clusters along the primary strand. Reading a cluster is a spatial sweep from cord $0$ to cord $N$. The *khipukamayuq*’s tactile finger resting on the active strand is **`I`** (the current index cord). When the boundary is reached, the loop exits smoothly with zero stack residue.
* **The Ayni Reciprocity Equilibrium (`BEGIN ... UNTIL` & `BEGIN ... WHILE ... REPEAT`):**
  In Andean economic and cosmic governance (*Ayni*), transactions and lifecycles iterate until harmonic balance is locked ($27 == 27$, $38 == 38$). In systems programming, this is the living pulse: polling serial hardware (`BEGIN KEY? UNTIL`), state machines, and cellular lifecycles with zero hidden stack frames.
* **Moiety Bifurcation Gates (`IF ... ELSE ... THEN`):**
  Branching represents the clean division between Hanan (Upper / True) and Hurin (Lower / False) pathways, converging back at the common knot (`THEN`).

---

## III. The Philosophy: *Thinking Forth* with a Crystalline Mind

For every practitioner stepping into the Crucible, Leo Brodie’s classic ***Thinking Forth*** remains our sovereign compass:
* **Radical Factoring:** Write small, sharp, single-purpose words (1 to 3 lines).
* **Explicit Stack Contracts:** Design around clean stack signatures `( before -- after )`.
* **Domain Vocabulary:** Grow the language upward into the living grammar of your problem.

However, Regulus introduces **seven crucial points of departure** where classical Forth habits give way to crystalline systems architecture:

```text
┌─────────────────────────────────┬──────────────────────────────────┬────────────────────────────────────────────────────────┐
│ Dimension                       │ Traditional Forth (FIG / 79 / 83)│ Regulus Crystalline Forth (v2.0.0)                     │
├─────────────────────────────────┼──────────────────────────────────┼────────────────────────────────────────────────────────┤
│ 1. System Topology              │ Monolithic Dict (Fused Kernel)   │ Cellular Organs (Decoupled Bytecode & Runners)         │
│ 2. Standalone App Delivery      │ Complex Target Meta-compilation  │ 1-Line Surgical Emission (EMIT-COM MYAPP.COM ENTRY)    │
│ 3. Introspection & Memory       │ Source Amnesia / Memory Dumps    │ Live Decompilation (INSPECT decompiles RAM to source)  │
│ 4. Documentation Strategy       │ Paper Manuals / Sparse Help      │ 113-Page Resident RGM2 Archive (Zero RAM / No Disk)    │
│ 5. Stack & Boundary Safety      │ Vulnerable to SP/RS Collision    │ Hardware Index Stack (IX) & Inverted-T Centroid Traps  │
│ 6. Dictionary Lifecycle         │ Destructive Rollback (FORGET)    │ Non-Destructive MARKER Snapshots & FREEZE Mitosis      │
│ 7. Control Flow Geometry        │ Opaque Jump Labels / Addresses   │ Khipu Spatial Sweeps (DO/LOOP/I) & Ayni Flow (BEGIN)   │
└─────────────────────────────────┴──────────────────────────────────┴────────────────────────────────────────────────────────┘
```

---

## IV. The Complete 113-Word Vocabulary Reference

```text
Stack & Dual-Stack:
  DUP ( n -- n n )          DROP ( n -- )             SWAP ( n1 n2 -- n2 n1 )
  OVER ( n1 n2 -- n1 n2 n1 ) ROT ( n1 n2 n3 -- n2 n3 n1 ) NIP ( n1 n2 -- n2 )
  TUCK ( n1 n2 -- n2 n1 n2 ) ?DUP ( n -- 0 | n n )    CLEAR ( ... -- )
  2DUP ( d -- d d )         2DROP ( d -- )            2SWAP ( d1 d2 -- d2 d1 )
  2OVER ( d1 d2 -- d1 d2 d1 )

Arithmetic, Logic & Math:
  + ( n1 n2 -- sum )        - ( n1 n2 -- diff )       * ( n1 n2 -- prod )
  / ( n1 n2 -- quot )       MOD ( n1 n2 -- rem )      /MOD ( n1 n2 -- rem quot )
  */ ( n1 n2 n3 -- quot )   */MOD ( n1 n2 n3 -- rem quot )
  1+ ( n -- n+1 )           1- ( n -- n-1 )           2* ( n -- n*2 )
  2/ ( n -- n/2 )           ABS ( n -- |n| )          MIN ( n1 n2 -- min )
  MAX ( n1 n2 -- max )      NEGATE ( n -- -n )        INVERT ( u -- ~u )
  AND ( u1 u2 -- u )        OR ( u1 u2 -- u )         XOR ( u1 u2 -- u )

Relational Comparisons:
  < ( n1 n2 -- flag )       > ( n1 n2 -- flag )       = ( n1 n2 -- flag )
  <> ( n1 n2 -- flag )      0= ( n -- flag )          0< ( n -- flag )
  0> ( n -- flag )          <= ( n1 n2 -- flag )      >= ( n1 n2 -- flag )

Control Flow & Structured Loops:
  IF ( flag -- ) ... ELSE ... THEN                    DO ( limit start -- ) ... LOOP
  BEGIN ... UNTIL ( flag -- )                         BEGIN ... AGAIN
  BEGIN ... WHILE ( flag -- ) ... REPEAT              I ( -- index )

Console, Strings & Hardware I/O:
  . ( n -- )                .S ( -- )                 EMIT ( c -- )
  CR ( -- )                 SPACE ( -- )              SPACES ( n -- )
  ." <text>" ( -- )         STYPE ( addr len -- )     KEY ( -- c )
  KEY? ( -- flag )          INP ( port -- byte )      OUTP ( byte port -- )

Block Memory & Arrays:
  FILL ( addr len c -- )    CMOVE ( src dst u -- )    MOVE ( src dst u -- )
  @ ( addr -- val )         ! ( val addr -- )         C@ ( addr -- byte )
  C! ( byte addr -- )       +! ( n addr -- )

Number Bases & Prefixes:
  HEX ( -- )                DECIMAL ( -- )            DEC ( -- )
  BASE ( -- addr )          $HEX-LITERAL              #DEC-LITERAL

Dynamic Dictionary Arena:
  : <name> ... ;            VARIABLE <name>           CONSTANT <name>
  MARKER <name>             HERE ( -- addr )          ALLOT ( n -- )
  , ( n -- )                C, ( byte -- )            WORDS ( -- )
  INSPECT <name>

Living Reproduction & Standalone Emission:
  FREEZE <image.com>        EMIT-COM <file.com> <word>
  EMIT-CRYSTAL <file.cry> <word>                      EMIT-VM <runner.com>

Virtual Disk Block Subsystem:
  BLOCK ( u -- addr )       BUFFER ( u -- addr )      UPDATE ( -- )
  FLUSH ( -- )              LIST ( u -- )             LOAD ( u -- )
  THRU ( u1 u2 -- )         WIPE ( -- )

Host CP/M 2.2 / MP/M II OS Environment:
  DIR [d:pat]               TYPE <file>               STAT [d:file]
  RUN <file> [args]         ERA <file>                CP <src> <dst>
  REN <new>=<old>           USER [0-15]               SAVE <n> <file>
  MAN [topic]               BYE
```

---

## V. Memory Architecture & Containment Safety

```text
0000H .. 00FFH (256 B)    Page Zero (Vectors, Default FCB, DMA Buffer)
0100H .. 8637H (34,104 B) Native Z80 VM Engine & In-Place Forth Bytecode
8638H .. B201H (11,210 B) 113-Page RGM2 v2 Archive (10.9 KiB, Zero RAM Stream)
B202H .. B98CH (1.9 KB)   Variables & Dynamic HERE Dictionary (T_STOP Ceiling)
BA80H                     Data Stack Top (Hardware Register IX grows downward)
BB00H .. BD00H (512 B)    Relocated RUN Loader Membrane & BDOS Gate
BFF0H                     Call Stack Top ((0006H) - 16 bytes headroom)
══════════════════════════════════════════════════════════════════════════════════
C000H .. FFFFH (16.0 KB)  MP/M II V2.1 Common Memory Segment (100% UNTOUCHED)
```

---

## VI. Distribution & Verification

```bash
# Verify the SHA-256 integrity of the distribution binary
sha256sum REGULUS.COM
# Expected: c60aea1111deb5492ae91ce0d0b018db6b3b756c70a5b2c74745f26f2cc3471c
```

May this small machine serve your journey into the boundless stacks of the cosmos.
