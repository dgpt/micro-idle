# **Game Design Prompt: “Micro-Idle”**

Design an **idle / hover-to-destroy game themed around microbiology**, where the player observes a **top-down 3D field of microbes**, destroys them through hovering, clicking, and abilities, and harvests **biologically realistic resources** that drive a deeply interlocked progression system.

The core fantasy is that **biology itself is the tech tree**: every mechanic, resource, enemy behavior, and visual change is grounded in microbial structure or function.

---

## **Core Player Loop**

1. The game continuously spawns **procedurally generated microbes** based on the current progression state.
2. The player destroys microbes via hovering, clicking, or automated effects.
3. Destroyed microbes release **multiple types of resources**, determined by their biological traits.
4. Resources are spent to unlock **new biological traits**, nutrient upgrades, and disinfection abilities.
5. Unlocks meaningfully change:

   * how microbes look
   * how they behave
   * what they drop
   * how the player interacts with them

There are **no cosmetic-only upgrades**: every unlock alters gameplay.

---

## **Microbes & Procedural Variety**

Microbes are generated from a deterministic combination of:

* a base form
* unlocked biological traits
* weighted variation parameters

### **Microbe Types**

The game features diverse microbe body plans, each with unique structure, behavior, and physics:

#### **Protists (Complex Eukaryotes)**
* **Amoeba** - Blob-like body with thin pseudopod extension, highly deformable
* **Stentor** - Trumpet-shaped ciliate with contractile body
* **Lacrymaria** - Body with extremely long extendable neck (up to 7× body length)
* **Vorticella** - Bell-shaped body with contractile stalk
* **Didinium** - Barrel-shaped predator with oral proboscis
* **Heliozoa** - Spherical core with radiating axopodia (thin spikes)
* **Radiolarian** - Geometric silica skeleton with radiating spines
* **Diatom** - Rigid silica shell (frustule) in various geometric shapes

#### **Bacteria (Prokaryotes)**
* **Coccus** - Spherical
* **Bacillus** - Rod-shaped
* **Vibrio** - Curved
* **Spirillum** - Spiral
* Flagellated variants for locomotion

#### **Viruses**
* Icosahedral capsids
* Bacteriophages with tail fibers

Each microbe type has a unique body plan, constraint configuration, and behavioral characteristics that become more complex with progression.

### **Progressive Biological Layers**

As the player progresses, microbes visibly gain new layers and structures, such as:

* membranes and protective capsules
* internal components (nucleoid regions, ribosomes, storage granules)
* external appendages (flagella, pili, EPS filaments)
* signaling or weaponized structures (toxin auras, harpoons, pulse emitters)

### **Behavioral Motion**

Microbes exhibit **procedural, readable motion** tied to their biology:

* membrane undulation
* flagella wave propagation
* pili twitching
* extracellular filament drift
* periodic signal pulses
* spore dormancy and reactivation

Motion communicates state and threat level rather than serving as pure spectacle.

### **Physics: Soft-Body Simulation**

Microbes are physically simulated as **deformable soft bodies** using **Bullet Physics** with OpenCL GPU acceleration:

* **Particle-based soft bodies**: Each microbe consists of nodes (particles) connected by distance constraints
  - Amoeba: 16 skeleton (internal structure) + 16 membrane (elastic outer layer) = 32 nodes
  - Other types have specialized configurations (Stentor: trumpet shape, Lacrymaria: extendable neck chain)
* **Skeleton structure**: Internal nodes arranged to drive shape and maintain integrity
* **Membrane layer**: Elastic outer nodes that deform, compress, and recover naturally
* **Distance constraints**: Links between nodes create soft-body behavior with configurable stiffness
* **Inter-microbe collisions**: Cause visible squishing and deformation when surfaces overlap
* **Collision response**: Soft and bouncy - microbes compress like gel, then separate smoothly
* **Material properties**: Different microbe types have different stiffness/squishiness (amoebas soft, diatoms rigid)
* **GPU acceleration**: Bullet's OpenCL solver runs physics on GPU for performance at scale

**Amoeba Locomotion (EC&M Model)**:
Amoebas use the **Excitable Cortex & Memory (EC&M)** algorithm for realistic pseudopod-based movement:
- 12-second behavioral cycle with phases: extend (0-35%), search (35-75%), retract (75-100%)
- One thin pseudopod extends at a time (not fat bulging)
- Lateral wiggle during search phase creates characteristic zig-zag motion
- Forces applied to membrane nodes drive Bullet soft body naturally

**Collision Behavior Requirements**:
- Microbes MUST collide when visual surfaces touch (collision radius matches rendered size)
- Response MUST show visible squishing and elastic deformation
- After collision, microbes MUST bounce apart gently, not stick or penetrate
- Collisions feel like soft gel bodies bumping into each other

This creates microbes that feel **alive and tactile** rather than rigid sprites.

---

## **Resources (Biologically Grounded)**

The game features a limited but meaningful set of resources:

**Sodium, Glucose, Iron, Calcium, Lipids, Oxygen, Signaling Molecules**

Key rule:

> **A resource does not exist in the game until the biological trait that justifies it has been unlocked.**

Resources are earned through destruction, passive generation, chain reactions, or group behaviors, depending on progression.

---

## **Progression System 1: DNA Traits (Discovery Tree)**

The primary progression system is a **prerequisite-based trait tree** representing biological discoveries.

Each trait unlock provides **exactly four things**:

1. One new resource type
2. One new drop opportunity
3. One new visible biological structure
4. One new gameplay mechanic

Example trait mappings:

* **Capsule → Sodium**
  Introduces survivability, shielding, and chain-reaction mechanics.
* **Flagella → Glucose**
  Introduces movement, clustering, and spawn density changes.
* **Pili → Iron**
  Enables adhesion, clumping, and elite microbe variants.
* **Endospores → Calcium**
  Introduces dormancy, delayed rewards, and burst windows.
* **LPS Layer → Lipids**
  Adds toxins, resistance, and pickup interactions.
* **Photosynthesis → Oxygen**
  Enables ambient resource generation and burn interactions.
* **Quorum Sensing → Signaling Molecules**
  Enables group coordination, chaining, and synchronized behavior.

Later traits unlock complex interactions such as:

* swarming
* chemotaxis
* conjugation
* coordinated attacks
* area-based effects

---

## **Progression System 2: Nutrients (Scaling & Optimization)**

Once unlocked via DNA traits, resources can be **freely invested** into nutrient upgrades.

These upgrades:

* scale numerically
* stack multiplicatively
* do not unlock new mechanics on their own

Examples:

* **Sodium:** larger or longer chain reactions
* **Glucose:** increased damage output and spawn rates
* **Iron:** drop multiplication beyond 100%, elite frequency
* **Calcium:** armor phases, survivability scaling
* **Lipids:** toxin resistance, pickup attraction
* **Oxygen:** burn effects, faster pacing
* **Signals:** cooldown reduction, synergy amplification

Late-game introduces **resource conversion and cross-feeding** so no resource remains isolated.

---

## **Progression System 3: Disinfection (Player Capabilities)**

This system defines **how the player interacts with the microbial field** and supports multiple playstyles.

### **Active Abilities**

Direct, skill-driven actions:

* aimed bursts
* beams
* chain detonations

### **Passive Effects**

Environmental control and area influence:

* acid fields
* pulses
* debuff zones

### **Idle / Automation**

Systems that act without player input:

* autonomous killers
* fever waves
* offline digestion or processing

Abilities across categories are designed to **synergize**, allowing hybrid builds.

---

## **Enemy Threats & Counterplay**

As progression advances, microbes gain **functional defenses and offenses**, not abstract difficulty scaling.

### **Defensive Structures**

* Capsules: damage mitigation
* Biofilms: armor + clustering
* Spores: near-invulnerability with delayed payoff

### **Offensive Structures**

* Harpoon systems: burst damage
* Contact toxins: damage-over-time auras
* Swarm coordination: group buffs and behaviors

All threats are:

* visually apparent
* mechanically distinct
* counterable through progression choices

---

## **Tone & Design Intent**

* Educational, but never didactic
* Biology is presented through interaction, not exposition
* Visual complexity increases with system depth, not noise
* The game rewards understanding systems, not memorization

**Micro-Idle is an idle game where learning how microbes work is how you win.**

---

## **Development Principles**

### **Primary Goal: Complexity Reduction**

The primary goal of all programming work is **complexity reduction while achieving requirements**. Clean, understandable code is not optional.

### **Core Practices**

* **No band-aid fixes** - Find root causes, don't suppress symptoms with emergency checks or arbitrary clamps
* **Always build and test** - Verify every change works before moving on
* **Refactor constantly** - Take every opportunity to clean, simplify, and improve code structure
* **Keep files manageable** - No thousand-line files. Split, organize, and maintain clear boundaries
* **Try different approaches** - If something doesn't work, try another algorithm. If still stuck, refactor until you find the code smell
* **Use proven solutions** - Utilize external libraries and established techniques over custom implementations
* **Clean as you go** - Every change should reduce complexity, not add special cases
* **Always be improving** - Actively identify areas for cleanup and refactoring

---

## **Technical Implementation**

### **Language & Stack**
* **Language**: C++17 for clean modern code
* **Framework**: raylib 5.5 (window/input/rendering)
* **Physics**: Bullet Physics 3.25 with OpenCL GPU solver
* **Graphics**: OpenGL 4.6 (via Mesa Zink on Linux)
* **Build**: CMake with FetchContent for dependencies

### **Rendering System**
Microbes are rendered using **deformable metaballs** for organic appearance:
* **Skeleton particles** generate metaball field (additive influence)
* **Membrane particles** modulate boundary and create smooth surface
* **Fragment shader** adds cellular details:
  - Semi-transparent membrane with thickness gradient
  - Internal organelles (skeleton particles visible through membrane)
  - Pseudopod tips highlighted (brighter/more transparent)
  - Nucleus region at center with distinct coloring
* **Metaball blending** creates smooth flowing boundaries that deform naturally
* **GPU instancing** renders thousands of particles efficiently

**Type-specific rendering**:
- Soft types (amoeba, stentor): Full metaball blending
- Spiked types (heliozoa, radiolarian): Thin line extensions for spines
- Rigid types (diatom): Geometric shell rendering
- Cilia types: Motion blur or flow lines

This approach naturally shows:
- Internal structure through semi-transparent membrane
- Thin pseudopod extensions (field strength varies with deformation)
- Organic blob-like shapes that compress and recover
- Diverse microbe morphologies with single unified system
