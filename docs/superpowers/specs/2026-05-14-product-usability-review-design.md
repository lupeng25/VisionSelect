# VisionSelect Product Usability Review Design

## Purpose

Review VisionSelect as a product for machine-vision engineers, system integrators, and internal R&D users. The review should answer one question: does the current software support fast, credible, and repeatable camera/lens/light pre-selection, or are there usability gaps that reduce engineering confidence?

This is a research and assessment task only. It does not change application behavior.

## Audience Assumption

The primary users are:

- Machine-vision engineers and integrators who understand FOV, working distance, pixel density, lenses, lighting, and industrial constraints.
- Internal R&D or application engineers who maintain catalogs and need a quick pre-selection tool.

The product does not need a beginner-oriented wizard for sales users. It should instead prioritize speed, traceability, data quality, and engineering review support.

## External References

The review will compare VisionSelect against adjacent open-source and public machine-vision tools rather than exact competitors, because no direct open-source equivalent was found for combined camera/lens/light selection.

- OpenPnP vision workflow: camera, lighting, fiducial, Z-height, and calibration concerns in real machine setups.
- Caliscope and Kalibr: calibration-first tools that emphasize intrinsic/extrinsic parameters, reprojection error, and quality feedback.
- Harvester: GenICam acquisition tooling that shows what real industrial camera integration involves.
- Lensfun: a maintainable lens-property database and correction model.
- Optiland: a high-end optical modeling reference for ray tracing, MTF/PSF, distortion, tolerancing, and optimization.
- Public machine-vision lens/FOV calculators from 1stVision, Machine Vision Store, Towin, and e-con Systems.

Key source links:

- https://github.com/openpnp/openpnp/wiki/Vision-Solutions
- https://github.com/mprib/caliscope
- https://github.com/ethz-asl/kalibr
- https://github.com/genicam/harvesters
- https://github.com/lensfun/lensfun
- https://github.com/optiland/optiland
- https://www.1stvision.com/cameras/lens-calculator/8192/1/7/7
- https://machinevisionstore.com/design/calculator
- https://www.towinlens.com/how-to-calculate-fov-for-machine-vision-fov-calculation.html
- https://www.e-consystems.com/calculator/calculate-lens-fov-and-focal-length.asp

## Review Dimensions

### 1. Engineer Workflow Completeness

Assess whether the app supports the expected engineering path:

- Define inspection requirements.
- Estimate camera and lens needs.
- Select and compare combinations.
- Understand recommendation reasons.
- Review risks and constraints.
- Export or document a candidate solution.
- Maintain camera, lens, and light catalogs.

### 2. Recommendation Credibility

Check whether recommendations explain the engineering basis clearly enough for a knowledgeable user:

- FOV coverage.
- Pixel density and smallest feature/defect relationship.
- Working distance and focal length fit.
- Lens image circle versus sensor size.
- Distortion and DOF risks.
- Camera frame rate, interface bandwidth, bit depth, dynamic range, and shutter risks.
- Light effective area, lighting mode, wavelength/color, and scene fit.

### 3. Internal Tool Efficiency

Assess whether frequent users can work quickly:

- Input forms avoid redundant work.
- Tables support scanning and comparison.
- Catalog import/export is reliable and visible.
- Filtered export behaves predictably.
- Reports can be reused as engineering artifacts.
- The UI makes catalog and selection state easy to understand.

### 4. Error Prevention

Identify whether the product prevents common machine-vision selection mistakes:

- Unit mistakes.
- Insufficient pixel density.
- Sensor/lens coverage mismatch.
- Impossible or fragile working distance.
- Interface bandwidth mismatch.
- Rolling shutter or exposure-risk mismatch.
- Light coverage or mode mismatch.
- Confusing theoretical estimates with validated onsite performance.

### 5. Boundary And Verification Language

Assess whether the app clearly labels the boundary between:

- Theoretical pre-selection.
- Catalog-based scoring.
- Field validation.
- Camera/lens calibration.
- Final acceptance testing.

The review should highlight where the product should add disclaimers, validation checklists, or confidence labels.

## Proposed Assessment Approaches

### Recommended: Usability Heuristic Review

Review the current app against the five dimensions above and produce a ranked issue list. This is the fastest way to identify product problems without over-scoping into algorithm or code changes.

Trade-off: it gives strong product guidance but does not prove algorithm correctness.

### Alternative: Workflow Walkthrough

Run a realistic selection scenario end to end and document friction at each step. This gives concrete evidence from the actual UI.

Trade-off: it is more grounded, but depends on choosing representative scenarios and may miss broader catalog/data issues.

### Alternative: Benchmark Matrix

Create a comparison table against adjacent tools and calculators. This makes gaps easy to see, especially around calibration, reporting, and validation.

Trade-off: it can overemphasize features from tools that are not direct competitors.

## Recommended Review Output

The review should produce one written report, not code changes:

- Summary judgment: whether VisionSelect is usable as an engineering pre-selection tool.
- Strengths: what the current product already does well.
- Issues: grouped by severity and user impact.
- Missing workflows: important workflows not yet covered.
- Suggested improvements: short-term, medium-term, and later.
- Non-goals: features that should not be prioritized for this product audience.
- Evidence: references to screens, current workflows, code/modules when useful, and external benchmark examples.

## Non-Goals

This assessment will not:

- Implement UI or algorithm changes.
- Replace the selection engine review.
- Perform a full optical simulation audit.
- Validate real hardware performance.
- Build GenICam/camera acquisition integration.
- Turn the app into a beginner sales wizard.

## Success Criteria

The review is successful if it gives the team a clear product decision:

- Keep the current direction.
- Fix specific usability gaps before expanding features.
- Decide which engineering-confidence features matter most next.
- Avoid spending time on unrelated capabilities.

## Self-Review

- No implementation work is included.
- The target audience is explicit.
- The review dimensions are specific to machine-vision engineering use.
- The external references are adjacent benchmarks, not treated as direct competitors.
- The output format is concrete enough to become an implementation or research plan later.
