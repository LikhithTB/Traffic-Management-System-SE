# Traffic Management System (Simulation)

**Software Engineering Mini-Project – Team E16, Section E, PES University**

| Name | SRN |
|---|---|
| Likhith T Bommisetty | PES1UG24CS249 |
| Devashruth M | PES1UG24CS251 |
| Manish Bala | PES1UG24CS263 |
| Manoj PS | PES1UG24CS265 |

A C/C++ console simulation of vehicle flow at intersections using queue data structures, with signal-timing control, emergency-vehicle priority, congestion monitoring and traffic reports.

## Phases

| Phase | Folder |
|---|---|
| Phase 1 – Requirements, Test Plan, Architecture & Design | [`Phase1/`](Phase1/) |
| Phase 2 – Backlog, Sprint 1 & Sprint 2 | [`Phase2/`](Phase2/) |

## Project Management

- **Project board:** [TMS Simulation – Team E16](https://github.com/users/LikhithTB/projects/2)
- **Backlog:** 21 stories derived from the SRS (FR, NFR, SEC) with story points, sprint and owner – see [`Phase2/Part2_Backlog.md`](Phase2/Part2_Backlog.md)
- **Sprint 1:** 19–23 Oct – core working product
- **Sprint 2:** 26–30 Oct – remaining features, hardening, documentation
- **Workflow:** one branch per story → Pull Request (`Closes #n`) → owner review → merge; GitHub Actions builds every push and PR

## Tech Stack
C++17 · g++ 11+ · Ubuntu 22.04 / Windows 11 (MinGW-w64) · local file storage

## Build & Run
```bash
g++ -Wall -Wextra -std=c++17 -o tms src/*.cpp
./tms
```

## Repository Structure
```
Phase1/    SRS, Test Plan, Architecture & Design Spec, diagrams
Phase2/    Backlog documentation, screenshots, sprint demo videos
src/       Source code
.github/   GitHub Actions workflows
```
