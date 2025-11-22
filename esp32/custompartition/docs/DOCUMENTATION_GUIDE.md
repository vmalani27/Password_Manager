# Documentation Guide

How to navigate and use project documentation effectively.

## Documentation Structure

```
Project Documentation
│
├── README.md              [START HERE]
│   Purpose: Quick start, project overview, getting started
│   Audience: New contributors, users, stakeholders
│   Update: On major releases or feature changes
│
├── docs/TODO.md           [DAILY WORK]
│   Purpose: Current sprint tasks, active backlog
│   Audience: Development team, sprint planning
│   Update: Daily during active development
│
├── docs/ROADMAP.md        [STRATEGIC PLANNING]
│   Purpose: Long-term vision, phased development
│   Audience: Project lead, stakeholders, architects
│   Update: Every sprint review (2 weeks)
│
└── docs/ARCHITECTURE.md   [TECHNICAL REFERENCE]
    Purpose: System design, implementation details
    Audience: Developers, technical reviewers
    Update: When architecture changes or features added
```

---

## How to Use Each Document

### README.md - Project Entry Point

**Read this when:**
- First time joining the project
- Need to build and run the firmware
- Want high-level project overview
- Looking for quick start guide

**Contains:**
- Project description and status
- Hardware setup instructions
- Build and flash commands
- Current sprint goals summary
- Links to other documentation

**Maintenance:**
- Update "Current Sprint Goals" section every sprint
- Update "Status" when phase changes
- Keep build instructions current
- Update version number on releases

---

### TODO.md - Active Sprint Backlog

**Read this when:**
- Starting daily development work
- Planning sprint tasks
- Need to pick next task to work on
- Checking task status or blockers

**Contains:**
- Current sprint committed work
- User stories with acceptance criteria
- Story point estimates
- Task status (Not Started/In Progress/Done)
- Sprint burndown tracking
- Backlog for future sprints

**Agile Workflow:**

1. **Sprint Planning (Every 2 Weeks):**
   - Review backlog items
   - Select high-priority tasks for sprint
   - Estimate story points
   - Commit to sprint goal

2. **Daily Development:**
   - Pick highest priority "Not Started" task
   - Move to "In Progress"
   - Implement and test
   - Update status to "Done" when complete
   - Update sprint burndown

3. **Sprint Review (End of Sprint):**
   - Mark completed items
   - Move incomplete items to next sprint
   - Update retrospective section

**Maintenance:**
- Update daily during active sprint
- Add new tasks to backlog as identified
- Adjust story points based on actual effort
- Document blockers immediately

---

### ROADMAP.md - Product Strategy

**Read this when:**
- Planning multi-sprint work
- Making architectural decisions
- Prioritizing features
- Stakeholder reporting

**Contains:**
- Phase-based development plan
- Long-term goals and vision
- Success criteria per phase
- Risk management
- Decision log

**Strategic Planning:**

1. **Phase Planning:**
   - Each phase = multiple sprints (4-12 weeks)
   - Clear goals and deliverables
   - Dependencies identified
   - Success criteria defined

2. **Decision Making:**
   - Log major technical decisions
   - Document alternatives considered
   - Explain rationale

3. **Risk Management:**
   - Identify technical risks early
   - Plan mitigation strategies
   - Track dependency risks

**Maintenance:**
- Review every sprint retrospective
- Update phase completion status
- Add to decision log when making architecture choices
- Adjust timeline based on velocity

---

### ARCHITECTURE.md - Technical Specification

**Read this when:**
- Need implementation details
- Writing new features
- Debugging complex issues
- Conducting code review

**Contains:**
- System design diagrams
- Component specifications
- Security model
- Data flows
- API documentation
- Performance characteristics

**Reference Guide:**

1. **Understanding System:**
   - Start with high-level diagrams
   - Read security model section
   - Study component details relevant to your work

2. **Implementing Features:**
   - Check existing patterns
   - Verify API signatures
   - Follow security guidelines

3. **Troubleshooting:**
   - Review data flow diagrams
   - Check component interactions
   - Verify memory layouts

**Maintenance:**
- Update when adding new components
- Document API changes immediately
- Add diagrams for complex features
- Update performance metrics after optimization

---

## Workflow Examples

### Example 1: Starting a New Sprint

**Step 1:** Review ROADMAP.md
- Check current phase goals
- Understand strategic priorities

**Step 2:** Sprint Planning with TODO.md
- Review backlog items
- Select tasks aligned with phase goals
- Estimate story points
- Commit to sprint goal

**Step 3:** Update README.md
- Update "Current Sprint Goals" section
- Update version if needed

---

### Example 2: Daily Development

**Step 1:** Check TODO.md
- Find highest priority "Not Started" task
- Read user story and acceptance criteria
- Move to "In Progress"

**Step 2:** Reference ARCHITECTURE.md
- Find relevant component documentation
- Check API signatures
- Review security requirements

**Step 3:** Implement and Test
- Write code following patterns
- Test against acceptance criteria
- Update TODO.md status to "Done"

---

### Example 3: Making Architectural Decision

**Step 1:** Research Options
- Check ARCHITECTURE.md for existing patterns
- Review similar decisions in ROADMAP.md decision log

**Step 2:** Evaluate Against Goals
- Check ROADMAP.md phase objectives
- Consider security implications

**Step 3:** Document Decision
- Add to ROADMAP.md decision log
- Update ARCHITECTURE.md with new design
- Create TODO.md tasks for implementation

---

### Example 4: Sprint Review

**Step 1:** Review TODO.md
- Mark all completed items
- Calculate velocity (completed story points)
- Update retrospective section

**Step 2:** Update ROADMAP.md
- Mark completed phase deliverables
- Adjust timeline if needed
- Update risk status

**Step 3:** Update README.md
- Update sprint summary
- Update version if releasing
- Update status indicators

---

## Document Relationships

### Information Flow

```
ROADMAP.md (Strategy)
      ↓ breaks down into
TODO.md (Tactics)
      ↓ references
ARCHITECTURE.md (Implementation)
      ↑ feeds back
      ↓ summarized in
README.md (Overview)
```

### Update Cascade

When you make changes:

1. **Code Change:**
   - Update TODO.md (mark task done)
   - Update ARCHITECTURE.md (if design changed)
   - Update README.md (if user-facing)

2. **Feature Complete:**
   - Update TODO.md (sprint burndown)
   - Update ROADMAP.md (phase progress)

3. **Sprint End:**
   - Update README.md (new sprint goals)
   - Update TODO.md (new sprint plan)

4. **Phase Complete:**
   - Update ROADMAP.md (phase status)
   - Update README.md (version, status)

---

## Priority System

### Task Prioritization

**P0 (Critical):**
- Security vulnerabilities
- Data corruption risks
- Blocking other work
- Sprint-committed work

**P1 (High):**
- Important security improvements
- Performance issues
- Key features

**P2 (Medium):**
- Code quality improvements
- Refactoring
- Nice-to-have features

**P3 (Low):**
- Polish items
- Future enhancements
- Nice-to-have improvements

### When Priority Conflicts

1. Security always wins over features
2. Data integrity over performance
3. Sprint-committed over backlog
4. Blocking over non-blocking

---

## Story Point Estimation

**1 Point:** < 2 hours, well-understood, no risk  
**2 Points:** 2-4 hours, mostly understood, low risk  
**3 Points:** 4-8 hours, some unknowns, medium risk  
**5 Points:** 1-2 days, significant unknowns, higher risk  
**8 Points:** 2-3 days, complex, should be broken down  
**13 Points:** > 3 days, definitely break into smaller tasks

**Rule of Thumb:** If > 8 points, split into sub-tasks.

---

## Common Questions

**Q: Which document do I read first?**  
A: README.md for overview, then TODO.md for current work.

**Q: Where do I track bugs?**  
A: Add to TODO.md backlog with "Bug: " prefix and appropriate priority.

**Q: How often should I update TODO.md?**  
A: Daily during active sprint, especially task status changes.

**Q: When do I update ARCHITECTURE.md?**  
A: When you add/modify components, APIs, or system design.

**Q: What if my task is blocked?**  
A: Update TODO.md with blocker description, pick next highest priority task.

**Q: How do I propose a new feature?**  
A: Add to TODO.md backlog, discuss in sprint planning, get prioritized.

**Q: Where do I document API changes?**  
A: ARCHITECTURE.md for details, TODO.md for migration tasks if needed.

**Q: What if actual effort differs from estimate?**  
A: Normal! Adjust future estimates, document in sprint retrospective.

---

## Document Templates

### User Story Template (TODO.md)

```markdown
#### US-XXX: Brief Title
**Priority:** P0/P1/P2/P3
**Story Points:** 1-13
**Status:** Not Started/In Progress/Done

**User Story:**
As a [role], I need [feature] so that [benefit].

**Acceptance Criteria:**
- [ ] Criterion 1
- [ ] Criterion 2

**Technical Notes:**
[Code snippets, API references, etc.]

**Definition of Done:**
- Criteria met
- Tests pass
- Documentation updated
```

### Decision Template (ROADMAP.md)

```markdown
**Date:** YYYY-MM-DD
**Decision:** What was decided
**Context:** Why decision was needed
**Alternatives:** What else was considered
**Rationale:** Why this choice was made
**Consequences:** Impact on project
```

---

## Best Practices

**DO:**
- Update TODO.md task status immediately
- Document architectural decisions in ROADMAP.md
- Keep README.md build instructions current
- Write clear acceptance criteria
- Link between documents when appropriate

**DON'T:**
- Copy/paste between documents (link instead)
- Let TODO.md get stale (update daily)
- Add implementation details to README.md (use ARCHITECTURE.md)
- Skip retrospectives (valuable learning)

---

## Maintenance Schedule

**Daily:**
- TODO.md task status updates

**Weekly:**
- TODO.md sprint burndown check
- Blocker review

**Biweekly (Sprint End):**
- TODO.md sprint retrospective
- ROADMAP.md progress update
- README.md sprint goals update

**Monthly:**
- ARCHITECTURE.md completeness review
- ROADMAP.md timeline adjustment
- All documents consistency check

---

## Getting Help

**For documentation questions:**
- Check this guide first
- Ask project lead during sprint planning
- Propose improvements via TODO.md task

**For technical questions:**
- Check ARCHITECTURE.md component details
- Review code comments
- Ask during code review

**For priority questions:**
- Check TODO.md tier assignments
- Discuss during sprint planning
- Escalate to project lead if unclear
