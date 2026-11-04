# Cast meeting presentation (unpublished local development)

This current contract supersedes the earlier unpublished entry/suspension design.
See [control refresh](49-cast-control-refresh.md). No release tag is created.

`layoutMode` is `grid` or `presentation`; `contentAdaptation` is independently
`fixed` or `auto-fit`. There is no public `presentationEnabled` feature gate.
Grid is a layout, not a view. In Presentation, `meeting.view` is `sidebar` or
`focus`; omit it and `primary` in Grid. Focus never changes native fullscreen.

`cast.setPresentationFocus` uses the existing exact P0 target, epoch, command
identity, bounded operation query and `expectedMeetingRevision` checks. `display`
is explicitly `sidebar`, `grid`, or `focus`, never toggle. `grid` and `focus`
require the exact current primary. `sidebarPage` is permitted only with `sidebar`
and must be validated against the resulting preview count before any mutation.
`supportsFocusView` declares Focus support. Invalid targets/pages/revisions do
not mutate anything. Source bindings, clocks and admissions are preserved.

P1 exposes six independent preferences: `presentationAutoEnter` (layout entry),
`presentationMode` (primary selection), `presentationSidebar`,
`presentationAudioFollow`, `presentationFocusOnDoubleClick` and
`presentationGridOnDoubleClick`. Each uses the one-field patch, explicit persist,
dedup-before-CAS, save-before-configure and independent settings barrier.
NearCast defaults to Presentation, Auto-fit, auto-entry on, Automatic primary
selection, Right, Follow on, Focus on, Grid cycle off. Its shipping profile does
not persist receiver preferences. Startup keeps Presentation selected while no
source exists; `primary` is absent until the first committed presentation.

Automatic primary selection chooses a genuinely new admission only while in
Presentation; Manual retains a live chosen primary. Either mode replaces a
departed primary with a remaining source. Layout auto-entry is independent:
while in Grid, two or more presented sources enter Sidebar when auto-entry is
enabled and not suppressed. Enabling it with two or more sources enters
immediately. An explicit Grid command or gesture suppresses entry for the
current nonempty source round without changing the preference. Complete source
withdrawal returns to Grid and clears that suppression: one source in the next
round stays Grid, the second enters Sidebar. Disabling auto-entry does not exit
Presentation; explicitly re-enabling clears suppression. `meeting.autoEnter`
and transient `meeting.autoEnterSuppressed` distinguish preference and activity.
Never persist the suppression. Legacy implementations omitting autoEnter do not
advertise the new setting; clients must use capability fields.

Selecting Presentation explicitly uses the remembered live primary, otherwise
the most recent presented source. Focus uses the same primary policy, with no
separate automatic/manual switch. Preview geometry remains fixed 84/16.

Grid source double-click enters Presentation Sidebar with that primary. Sidebar
preview double-click replaces primary. Main-body double-click follows:

| Focus entry | Include Grid | Sidebar main | Focus main |
| --- | --- | --- | --- |
| true (default) | false (default) | Focus | Sidebar |
| false | true | Grid | Sidebar |
| true | true | Focus | Grid |
| false | false | No mutation | Sidebar |

Disabling Focus while in Focus restores Sidebar. Explicit remote layout/view
commands are independent of these gesture preferences. A no-op does not advance
revisions. Sidebar left/right and paging preserve hidden sources and clocks.

Audio follow is independent of base automatic/manual selection. Exclusive uses
primary; mix gives an allowed ready primary .84 and the other selected ready
sources a total .16 divided equally; primary alone uses 1. Explicit per-source
mute is retained. Turning follow off or returning to Grid preserves selections
and restores ordinary weights. Independent `masterMuted` gates all output,
including later admissions, without destroying those selections. Gain updates
reduce before increases. Applied gain is not a perceptual loudness guarantee.

`presentationChanged` and `audioPolicyChanged` remain full P0 commit-group parts;
settings has its own barrier associated by `appliedControlRevision`. Never save
primary identities, source lists, gains or secrets. Focus requests declare JSON
only until an explicit TLV enum binding is adopted; runtimes must reject an
unbound enum instead of inventing a mapping. Real media/device acceptance is
separate from schema and reference-model checks.
