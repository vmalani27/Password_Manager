# Sprint Backlog

Active tasks for current development sprint. Organized by priority tier using Agile methodology.

## Document Purpose

This is the working task list for current sprint (2-week iteration). For long-term planning, see ROADMAP.md. For technical specifications, see ARCHITECTURE.md.

## Current Sprint (Sprint 3: Dec 1-15, 2025)

**Status:** In Progress

---

### Completed Stories
- [DONE] ECDH key exchange (ESP32 side)
- [DONE] Challenge-response authentication (ESP32 side)
- [DONE] Device binding with NVS (ESP32 side)
- [DONE] Database worker queue (FreeRTOS task, prevents SQLite corruption)
- [DONE] Quote-aware command parser
- [DONE] Emergency recovery (`db_reset` command, physical unpair button)

---

### In Progress
- [IN PROGRESS] Flutter client ECDH/session encryption (guide complete, code pending)
- [IN PROGRESS] Token size upgrade (32-bit → 128-bit)
- [IN PROGRESS] App resynchronization and crash recovery testing

---

### Not Started / Blocked
- [NOT STARTED] Remove plaintext passwords over BLE (encrypt with session key, AES-GCM planned)
- [NOT STARTED] Migrate AES-CBC to AES-GCM for database (authenticated encryption)
- [NOT STARTED] Dynamic PIN generation (replace static 123456)
- [NOT STARTED] Message authentication and replay protection (CTR → GCM, add sequence numbers)
- [NOT STARTED] Fix database key persistence (credentials lost on reboot)
- [NOT STARTED] Enable NVS flash encryption (sdkconfig)
- [NOT STARTED] Remove sensitive logging (tokens in serial)
- [NOT STARTED] Add backup/recovery mechanism

---

## Backlog (Not Committed)

### TIER B: Refactoring
- Split main.cpp into modules
- Normalize logging system
- Simplify BLE initialization

### TIER C: Cleanup
- Extract constants to config.h
- Add watchdog timer
- Add version info to advertising

---

## Sprint Burndown
**Total Committed:** 9 story points
**Completed:** 5 points
**Remaining:** 4 points

**Days Remaining:** 7 days  
**Target Velocity:** 8 points/sprint  
**On Track:** Yes (buffer of 1 point)

---

## Blockers

None currently.

---

## Sprint Retrospective Template

**What Went Well:**
- ECDH implementation completed ahead of schedule
- Device binding working reliably

**What Needs Improvement:**
- Need to start Flutter client work earlier
- Database worker queue more complex than estimated

**Action Items for Next Sprint:**
- Break down large tasks into smaller subtasks
- Allocate time for Flutter development in parallel

---

## Task States

**Not Started:** Task in backlog, not yet begun  
**In Progress:** Actively being worked on  
**Blocked:** Waiting on dependency or external input  
**In Review:** Code complete, awaiting testing  
**Done:** Acceptance criteria met, merged to main

---

## Definition of Ready (Task Prerequisites)

Before moving task to "In Progress":
- [ ] Acceptance criteria defined
- [ ] Technical approach documented
- [ ] Dependencies identified
- [ ] Story points estimated
- [ ] Assigned to developer

## Definition of Done (Task Completion)

Before marking task "Done":
- [ ] Code implements acceptance criteria
- [ ] Code compiles without warnings
- [ ] Manual testing passed
- [ ] No new security vulnerabilities introduced
- [ ] Documentation updated (if API changed)
- [ ] Committed to version control

---

## Document Maintenance

**Update Frequency:** Daily during active sprint  
**Owner:** Development Team  
**Last Updated:** November 22, 2025

**Update Triggers:**
- Task status changes
- New blockers identified
- Sprint planning completed
- Sprint retrospective held
