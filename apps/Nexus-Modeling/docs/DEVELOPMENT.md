# Nexus Modeling — Documentation Index

*Complete documentation map for developers, experts, and beginners.*

---

## 📚 Documentation Structure

```
docs/
├── README.md                           # Main entry point
├── reference-roadmap.md                # Strategic roadmap & competitive analysis
├── modeling-parity-index.md            # Feature parity matrix (112 features)
│
├── developer/                          # 👨‍💻 Developer Documentation
│   ├── index.html                      # Developer portal (HTML)
│   ├── architecture.md                 # System architecture overview
│   ├── kernel-api.md                   # Complete kernel API reference
│   ├── geometry-kernel.md              # Geometry kernel internals
│   ├── renderer.md                     # Vulkan renderer internals
│   ├── analyzer-brep.md                # Analytic B-rep deep dive
│   ├── boolean-csg.md                  # Boolean/CSG deep dive
│   ├── halfedge-mesh.md                # Half-edge mesh guide
│   ├── mesh-operations.md              # Mesh operations reference
│   ├── tolerance.md                    # Tolerance system
│   ├── adding-geometry-op.md           # Adding geometry operations
│   ├── adding-cad-feature.md           # Adding CAD features
│   ├── testing.md                      # Testing strategy
│   ├── build-system.md                 # CMake & build configuration
│   ├── porting.md                      # Platform porting guide
│   └── contributing.md                 # Contribution guidelines
│
├── expert/                             # 🎨 Expert User Guide
│   ├── index.html                      # Expert portal (HTML)
│   └── index.md                        # Advanced modeling, B-rep, CSG, parametric
│
├── beginner/                           # 👤 Beginner Guides
│   ├── index.html                      # Beginner portal (HTML)
│   └── getting-started.md              # Installation, first model, shortcuts
│
├── api/                                # 🔧 API Reference
│   ├── index.html                      # API portal (HTML)
│   └── index.md                        # Complete kernel API reference
│
├── html/                               # 🌐 Generated HTML Documentation
│   ├── styles.css                      # Shared stylesheet
│   ├── index.html                      # Main landing page
│   ├── developer/index.html            # Developer portal
│   ├── expert/index.html               # Expert portal
│   ├── beginner/index.html             # Beginner portal
│   └── api/index.html                  # API portal
│
├── modeling-parity-index.md            # Parity matrix (112 features)
├── reference-roadmap.md                # Strategic roadmap
└── DEVELOPMENT.md                      # This file
```

---

## 🎯 Quick Navigation by Audience

| Audience | Start Here | Key Documents |
|----------|------------|---------------|
| **New User** | `beginner/getting-started.md` | Installation → First Model → Core Concepts |
| **Expert User** | `expert/index.md` | Advanced Modeling → Boolean/CSG → Analytic B-rep |
| **Developer** | `developer/architecture.md` | Architecture → Kernel API → Renderer Internals |
| **Contributor** | `developer/contributing.md` | Code Style → PR Process → Tests |

---

## 📊 Documentation Stats

| Category | Files | Lines | Formats |
|----------|-------|-------|---------|
| Developer | 12 | ~15,000 | .md + HTML |
| Expert | 1 | ~3,000 | .md + HTML |
| Beginner | 2 | ~2,000 | .md + HTML |
| API Reference | 1 | ~4,000 | .md + HTML |
| Parity/Roadmap | 2 | ~8,000 | .md |
| HTML Templates | 5 | ~2,000 | .html + CSS |
| **Total** | **~20** | **~35,000** | **.md + .html + .css** |

---

## 🔗 Cross-References

All documents maintain bidirectional links:
- **In-text**: `[Mesh Operations](mesh-operations.md)`
- **TOC**: Auto-generated from headers
- **HTML**: Navigation bars + sidebar TOC
- **API**: Cross-linked types (`Mesh`, `Body`, `HalfEdgeMesh`)

---

## 🛠️ Regenerating HTML

```bash
# Install pandoc
sudo apt install pandoc

# Build all HTML
cd docs
for f in *.md developer/*.md expert/*.md beginner/*.md api/*.md; do
  pandoc "$f" -o "html/${f%.md}.html" --css=styles.css --toc --toc-depth=3
done

# Or use the build system
cmake -S . -B build -DNEXUS_BUILD_DOCS=ON
cmake --build build --target docs
```

---

## 📋 Maintenance Checklist

When adding new features:

- [ ] Update **modeling-parity-index.md** (feature matrix)
- [ ] Update **reference-roadmap.md** (roadmap status)
- [ ] Add **kernel-api.md** entry for new APIs
- [ ] Update **modeling-parity-index.md** feature matrix
- [ ] Add **test coverage** for new feature
- [ ] Update **changelog** (CHANGELOG.md)
- [ ] Regenerate HTML if structural changes

---

## 📁 File Naming Conventions

| Type | Pattern | Example |
|------|---------|---------|
| Markdown | `kebab-case.md` | `analytic-brep.md` |
| HTML | `kebab-case.html` | `analytic-brep.html` |
| CSS | `kebab-case.css` | `styles.css` |
| Code samples | `snake_case.cpp` | `analytic_brep.cpp` |

---

## 📋 Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.1.0-dev | 2026-07-14 | Initial comprehensive documentation set |

---

*Documentation maintained by the Nexus Modeling team. Last updated: 2026-07-14*