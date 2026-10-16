import QtQuick 2.0
import Sailfish.Silica 1.0
import QtQuick.LocalStorage 2.0 as Sql
import QtGraphicalEffects 1.0
import QtFeedback 5.0
import Nemo.KeepAlive 1.2
import "../js/stats.js" as Stats
import "../js/scrambleLayout.js" as Layout
import "../components"

Page {
    id: page

    // ---- font: Nunito (SIL OFL 1.1), bundled static TTFs (see fonts/OFL.txt) ----
    // Keep the display on for the whole solve (hold + run); normal blanking otherwise.
    DisplayBlanking { preventBlanking: page.phase === "holding" || page.phase === "running" }

    // Short haptic tick for long-press actions (copy scramble, rename session).
    ThemeEffect { id: buzz; effect: ThemeEffect.PressWeak }

    FontLoader {
        id: nunitoRegular
        source: Qt.resolvedUrl("../fonts/Nunito-Regular.ttf")
    }
    // Bold face: registered so font.bold resolves inside the Nunito family (not referenced by id).
    FontLoader {
        id: nunitoBold
        source: Qt.resolvedUrl("../fonts/Nunito-Bold.ttf")
    }
    // Family for all text (Bold is picked from the same family by font.bold).
    readonly property string appFont: (nunitoRegular.status === FontLoader.Ready && nunitoRegular.name !== "")
                                      ? nunitoRegular.name : Theme.fontFamily
    // Letter spacing as a fraction of the pixel size (0.04 em text, 0.02 em big time).
    // Linear in size, so the 100 px probes scale exactly like the real text.
    readonly property real trackK: 0.04
    readonly property real trackBigK: 0.02

    // WCA events, in picker order. ids are the event keys of the native scrambler (src/scramble).
    readonly property var events: [
        { id: "333",    name: "3\u00d73\u00d73" },
        { id: "222",    name: "2\u00d72\u00d72" },
        { id: "444",    name: "4\u00d74\u00d74" },
        { id: "555",    name: "5\u00d75\u00d75" },
        { id: "666",    name: "6\u00d76\u00d76" },
        { id: "777",    name: "7\u00d77\u00d77" },
        { id: "333oh",  name: "3\u00d73\u00d73 OH" },
        { id: "333bld", name: "3\u00d73\u00d73 BLD" },
        { id: "pyram",  name: "Pyraminx" },
        { id: "skewb",  name: "Skewb" },
        { id: "minx",   name: "Megaminx" },
        { id: "sq1",    name: "Square-1" },
        { id: "clock",  name: "Clock" }
    ]

    property int eventIndex: 0                       // 3x3 by default
    readonly property var currentEvent: events[eventIndex]

    // Sessions (csTimer-style groups of solves). Each session belongs to one event:
    //   {id, name: "Session N", event: <event id>, created: ms, solves: [solve...]}
    // Solve: {time_ms, penalty: {plus2Count: n, dnf: bool}, event, scramble}
    property var sessions: []
    property int currentSession: 0                   // index into sessions
    readonly property var currentSessionObj: sessions.length > currentSession ? sessions[currentSession] : null
    readonly property var sessionSolves: currentSessionObj ? currentSessionObj.solves : []
    // The last solve of the current session (the one OK/+2/DNF/Delete act on).
    readonly property var lastSolve: sessionSolves.length > 0 ? sessionSolves[sessionSolves.length - 1] : null
    readonly property bool canEditLast: lastSolve !== null && phase === "idle"
    // Pull-down only exists when the session has solves (nothing to act on otherwise).
    readonly property bool hasSolves: sessionSolves.length > 0
    readonly property string sessionName: currentSessionObj ? currentSessionObj.name : "Session 1"
    // ao5 / ao12 / ao100 of the current session (WCA rules, see js/stats.js).
    // Re-evaluated whenever sessionSolves changes (new solve, OK/+2/DNF/Delete, session switch).
    readonly property var stats: Stats.computeStats(sessionSolves)
    // Latest solve of the session ("time" row) and per-solve history (expanded table).
    readonly property string latestText: Stats.solveText(lastSolve)
    readonly property var history: Stats.rollingAverages(sessionSolves)
    // Session bests (single + best ao5/12/100), shown as the pinned right column of the open table.
    readonly property var bests: Stats.bestStats(sessionSolves)
    // The last solve is a new best single of this session (shown as "PB" under the time).
    readonly property bool lastIsPb: Stats.isNewBestSingle(sessionSolves)
    // Index of the session's best single: its time cell in the history table uses bestColor.
    readonly property int bestSingleIdx: Stats.bestSingleIndex(sessionSolves)
    // End index of the best ao5/ao12/ao100 window: those cells use bestColor too.
    readonly property var bestAoIdx: Stats.bestAoIndices(sessionSolves)
    // Stats block: collapsed (labels | latest values) or expanded to a history table.
    // expandT animates 0 -> 1 (200 ms, OutCubic) and drives width, header height and fades.
    property bool statsExpanded: false
    property real expandT: statsExpanded ? 1 : 0
    Behavior on expandT { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
    // Stats block sizes (px). Column widths come from the probe Texts in the flickable.
    readonly property real statsLabelW: Math.max(pl1.implicitWidth, pl2.implicitWidth, pl3.implicitWidth, pl4.implicitWidth)
    readonly property real statsValueW: Math.max(pvT.implicitWidth, pv5.implicitWidth, pv12.implicitWidth, pv100.implicitWidth)
    readonly property real statsCollapsedW: Math.min(page.width - 2 * Theme.horizontalPageMargin - Theme.paddingSmall,
                                                     statsLabelW + Theme.paddingSmall + statsValueW)
    readonly property real statsRowH: Math.round(Theme.fontSizeExtraSmall * 1.5)
    readonly property real histHeaderH: Math.round(Theme.fontSizeTiny * 1.6)
    readonly property real histColW: pHist.implicitWidth + Theme.paddingLarge
    readonly property real bestColW: Math.max(pBest.implicitWidth, pBestHead.implicitWidth) + Theme.paddingLarge - Theme.paddingMedium   // gap to #N = gap between history columns
    // Shown instead of the history when the session has no solves (one '-' column, no popup).
    readonly property var emptyHist: [{ n: "-", time: "-", ao5: "-", ao12: "-", ao100: "-" }]
    onExpandTChanged: if (statsExpanded) scrollHistToNewest();
    // Keep the history table on its newest column (right edge) while expanded.
    // Only called on expand and when the table was already following the newest column.
    function scrollHistToNewest() {
        if (expandT <= 0) return;
        histFlick.followNewest = true;
        histFlick.contentX = Math.max(0, histFlick.contentWidth - histFlick.width);
        histFlick.savedX = histFlick.contentX;
    }
    // Set by a press inside the expanded table: release collapses it unless the press
    // moved past the tap slop or the history flickable was moving (drag/flick scrolls).
    property bool tableTapPending: false
    property real tablePressX: 0
    property real tablePressY: 0
    // Scramble and stats: full when idle, dimmed while holding (armed), hidden while running.
    readonly property real idleOpacity: phase === "idle" ? 1 : (phase === "holding" ? 0.3 : 0)
    // Scramble: full while idle, dimmed while holding. On start a wipe (wipeMask) moves up
    // from the bottom edge; when it has passed, the scramble is hidden. Both are reset on stop.
    // True only during the solve-stop transition: scramble and stats switch back instantly
    // (no fade-in) while the wipe still covers them, so nothing flashes.
    property bool noFade: false
    // Scramble visibility is ONE explicit state, set by setScrambleMode() in a single step
    // (no binding, so no intermediate value between holding / running / idle):
    //   normal = opacity 1 (idle) | dim = 0.3 (holding) | sweep = 0.3 + wipe moving
    //   hidden = opacity 0, wipe finished (running, after the sweep) | back to normal on stop.
    property string scrambleMode: "normal"
    // Dim/hide goes on the scramble TEXT, not on scrambleBox: the wipe mask is a child of
    // scrambleBox and an opacity there would change the mask as well.
    // Top controls (event row + session row): ONE state, set in one place.
    //   normal = 1 (idle, after stop/cancel) | dim = 0.3 (holding) | hidden = 0 (running)
    // Opacity only (no layout change). Clicks are already blocked by phase (MouseAreas/BackgroundItems).
    // The event row (name + arrow) stays readable: 1 idle, 0.5 holding, 0.4 running.
    // The session row keeps its own rule: 1 idle, 0.3 holding, hidden while running.
    function setTopMode(m) {
        sessionRowItem.opacity = (m === "normal") ? 1 : (m === "dim" ? 0.3 : 0);
        eventButton.opacity = (m === "normal") ? 1 : (m === "dim" ? 0.5 : 0.4);
    }
    function setScrambleMode(m) {
        scrambleMode = m;
        scrambleText.opacity = (m === "normal") ? 1 : (m === "hidden" ? 0 : 0.3);
    }
    NumberAnimation {
        id: wipeAnim
        target: wipeShift
        property: "y"
        duration: 180
        easing.type: Easing.OutCubic
        onStopped: if (page.scrambleMode === "sweep") page.setScrambleMode("hidden");
    }

    property string scramble: ""

    // ---- copy scramble (long press on the scramble, main page or popup) ----
    readonly property int copyHoldMs: 700
    property bool copyFlash: false                   // scramble text shows the highlight colour
    function copyScramble(text) {
        if (!text || text === "(no scramble saved)") return;
        Clipboard.text = Layout.plainText(text);
        buzz.play();                                   // short haptic tick
        copyFlash = true;
        copyFlashTimer.restart();
        copyNotice.opacity = 1;
        copyNoticeTimer.restart();
    }
    property string timeText: "0.00"                 // big time
    property string subText: ""                      // DNF raw time or +2 total, under the big time
    property string phase: "idle"                    // idle | holding | running
    property bool ready: false                       // hold >= readyDelayMs
    property bool pinching: false                    // two-finger gesture in progress
    property real holdStart: 0
    property real startMs: 0
    property real lastStopMs: -100000

    readonly property int readyDelayMs: 550          // hold time before green
    // Red (not ready) phase: the app-wide drag threshold is raised to holdDragFactor x the
    // default (uiHints, set in main.cpp), so a small wobble does not let the page flickable
    // steal the touch; a real drag still does, and the pull-down menu follows it as usual.
    // Restored as soon as the hold turns green, starts, or is cancelled.
    readonly property int holdDragFactor: 3
    function setHoldDragSlop(on) {
        if (typeof uiHints === "undefined" || !uiHints.defaultDragDistance) return;   // offscreen harness
        uiHints.dragDistance = on ? uiHints.defaultDragDistance * holdDragFactor : uiHints.defaultDragDistance;
    }
    readonly property int cooldownMs: 300            // ignore touches after stop
    // Hold feedback colours (red = not ready, green = ready) are fixed; everything else is Theme.
    readonly property color notReadyColor: "#ff7b84"
    readonly property color readyColor: "#6fdc9a"
    readonly property color dimColor: Theme.secondaryColor
    readonly property color accentColor: Theme.highlightColor       // pressed, current item, '+', copy flash
    readonly property color lavenderColor: Theme.secondaryColor     // secondary text: session name, labels, arrows
    readonly property color bestColor: Theme.highlightColor         // session bests column (stands out)
    readonly property color textColor: Theme.primaryColor           // primary text: event title, times, list names
    // Raised surfaces: drop-down lists, the solve popup card, the row under a drop-down.
    readonly property color panelColor: Theme.rgba(Theme.highlightDimmerColor, 0.9)
    // Solve popup card: same tint as the panels but fully opaque.
    readonly property color popupCardColor: Theme.rgba(Theme.highlightDimmerColor, 1.0)
    // Press feedback fill (rows, '+' circle, event name): Silica's pressed background.
    readonly property color pressFill: Theme.rgba(palette.highlightBackgroundColor, Theme.highlightBackgroundOpacity)
    // One corner radius for every raised card: solve popup and both drop-down lists.
    readonly property real cornerRadius: Theme.paddingMedium

    // Longest text in a list model (drop-down width probe).
    function widestName(items) {
        var best = "";
        for (var i = 0; i < items.length; i++) {
            var n = String(items[i].name || "");
            if (n.length > best.length) best = n;
        }
        return best;
    }

    // ---- time font size -------------------------------------------------
    // Min: Theme.fontSizeExtraLarge. Default: 2.5 x Theme.fontSizeHuge.
    // Max: the largest size at which the widest time "9:59.99" still fits
    // within page width minus 2 * horizontalPageMargin (width scales linearly
    // with pixel size, so we measure at 100 px and scale).
    readonly property real minTimeSize: Theme.fontSizeExtraLarge
    readonly property real defaultTimeSize: Theme.fontSizeHuge * 2.5
    readonly property real maxTimeSize: {
        var avail = page.width - 2 * Theme.paddingMedium;
        if (sizeProbe.implicitWidth <= 0) return defaultTimeSize;
        // Fit both width (widest "9:59.99", bold) and the free height of timeRegion.
        return Math.max(minTimeSize,
                        Math.floor(Math.min(100 * avail / sizeProbe.implicitWidth,
                                            0.6 * timeRegion.height)));
    }
    property real timeSize: defaultTimeSize
    property real pinchBaseSize: defaultTimeSize

    function clampSize(v) {
        return Math.min(maxTimeSize, Math.max(minTimeSize, v));
    }

    // ---- scramble layout (renderer: components/ScrambleGrid.qml) --------
    readonly property real scrambleSideMargin: (currentEvent.id === "minx") ? Theme.paddingSmall : Theme.paddingMedium
    readonly property real scrambleMaxHeight: page.height * ((currentEvent.id === "minx") ? 0.38 : 0.35)
    readonly property real scrambleWidth: page.width - 2 * scrambleSideMargin

    // Penalty line under the time: about 40% of the time size, at least fontSizeLarge.
    readonly property real penaltySize: Math.max(Theme.fontSizeLarge, timeSize * 0.4)


    // "12.34" below 60 s, "m:ss.cc" from 60 s on. Truncates to centiseconds (WCA style).
    function fmt(ms) { return Stats.fmtMs(ms); }

    // ---- persistence (QtQuick.LocalStorage) -----------------------------
    property var db: null

    function openDb() {
        if (!db) db = Sql.LocalStorage.openDatabaseSync("cubetimer", "1.0", "Cube Timer", 1000000);
        db.transaction(function (tx) {
            tx.executeSql("CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT)");
        });
    }
    function getSetting(key, def) {
        var v = def;
        db.transaction(function (tx) {
            var r = tx.executeSql("SELECT value FROM settings WHERE key = ?", [key]);
            if (r.rows.length > 0) v = r.rows.item(0).value;
        });
        return v;
    }
    function setSetting(key, value) {
        db.transaction(function (tx) {
            tx.executeSql("INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)", [key, String(value)]);
        });
    }
    // Several keys in ONE transaction (all written or none).
    function setSettings(map) {
        db.transaction(function (tx) {
            for (var k in map)
                tx.executeSql("INSERT OR REPLACE INTO settings(key, value) VALUES(?, ?)", [k, String(map[k])]);
        });
    }
    // Stored sessions come from JSON. Drop malformed entries and fill in missing fields so the
    // rest of the code can rely on id, name, solves (time_ms, penalty) being present.
    // Dropped entries shift the stored currentSession index; it is only a fallback anyway.
    function sanitizeSessions(list) {
        var out = [];
        var maxId = 0;
        for (var i = 0; i < list.length; i++) {
            var s = list[i];
            if (!s || typeof s !== "object" || !Array.isArray(s.solves)) continue;
            var ok = [];
            for (var j = 0; j < s.solves.length; j++) {
                var v = s.solves[j];
                if (!v || typeof v.time_ms !== "number") continue;
                var p = v.penalty || {};
                v.penalty = { plus2Count: Math.max(0, parseInt(p.plus2Count, 10) || 0), dnf: !!p.dnf };
                ok.push(v);
            }
            s.solves = ok;
            if (typeof s.id === "number") maxId = Math.max(maxId, s.id);
            out.push(s);
        }
        for (var k = 0; k < out.length; k++) {
            if (typeof out[k].id !== "number") out[k].id = ++maxId;
            if (typeof out[k].name !== "string") out[k].name = "Session";
        }
        return out;
    }
    function loadSettings() {
        try {
            openDb();
            var id = getSetting("event", "333");
            for (var i = 0; i < events.length; i++) {
                if (events[i].id === id) eventIndex = i;
            }
            // Time font size is NOT persisted: every launch starts at the default size.
            timeSize = defaultTimeSize;

            var list = [];
            try { list = JSON.parse(getSetting("sessions", "[]")); } catch (e) { list = []; }
            if (!Array.isArray(list)) list = [];
            list = sanitizeSessions(list);
            sessions = list;
            // Per-event session counters: the highest "Session N" ever handed out for each event
            // (survives deletions). Initialised from the stored map and from the existing sessions
            // of that event; renamed sessions are ignored (their number is not known).
            var storedMap = {};
            try { storedMap = JSON.parse(getSetting("sessionCounters", "{}")) || {}; } catch (e) { storedMap = {}; }
            sessionCounters = {};
            for (var ek in storedMap) {
                var sv = parseInt(storedMap[ek], 10);
                if (!isNaN(sv) && sv > 0) sessionCounters[ek] = sv;
            }
            for (var m = 0; m < sessions.length; m++) {
                var ev = sessions[m].event;
                var mm = /^Session (\d+)$/.exec(sessions[m].name);
                if (ev && mm) sessionCounters[ev] = Math.max(sessionCounters[ev] || 0, parseInt(mm[1], 10));
            }
            var pc = parseInt(getSetting("peekCount", "0"), 10);
            peekCount = isNaN(pc) ? 0 : pc;
            // Last-used session per event. Legacy fallback (no map entry for this event yet):
            // the global currentSession index, as before.
            var lm = {};
            try { lm = JSON.parse(getSetting("lastSessionByEvent", "{}")) || {}; } catch (e) { lm = {}; }
            lastSessionByEvent = lm;
            var cs = parseInt(getSetting("currentSession", "0"), 10);
            if (lm[currentEvent.id] === undefined && !isNaN(cs) && cs >= 0 && cs < sessions.length && sessions[cs].event === currentEvent.id) {
                currentSession = cs;
                rememberSession();
            } else {
                ensureSession();
            }
        } catch (e) {
            console.log("cube-timer: settings not loaded: " + e);
            ensureSession();
        }
    }
    // Everything that changes with a session/solve/event is written here in one transaction.
    function persist() {
        lastSessionByEvent = lastSessionMap();
        try {
            setSettings({ event: currentEvent.id,
                          sessions: JSON.stringify(sessions),
                          currentSession: currentSession,
                          sessionCounters: JSON.stringify(sessionCounters),
                          lastSessionByEvent: JSON.stringify(lastSessionByEvent) });
        } catch (e) {
            console.log("cube-timer: settings not saved: " + e);
        }
    }

    // ---- sessions -------------------------------------------------------
    // Session numbers are per event: sessionCounters[eventId] = last "Session N" handed out for it.
    property var sessionCounters: ({})
    // Last-used session per event: lastSessionByEvent[eventId] = session id (stored in settings).
    property var lastSessionByEvent: ({})
    // lastSessionByEvent with the current session recorded (no write).
    function lastSessionMap() {
        var m = {};
        for (var k in lastSessionByEvent) m[k] = lastSessionByEvent[k];
        if (currentSession >= 0 && currentSession < sessions.length)
            m[sessions[currentSession].event] = sessions[currentSession].id;
        return m;
    }
    function rememberSession() {
        lastSessionByEvent = lastSessionMap();
        try { setSetting("lastSessionByEvent", JSON.stringify(lastSessionByEvent)); } catch (e) { console.log("cube-timer: lastSession not saved: " + e); }
    }
    function nextSessionNumber() {
        var ev = currentEvent.id;
        var n = (sessionCounters[ev] || 0) + 1;
        sessionCounters[ev] = n;
        try { setSetting("sessionCounters", JSON.stringify(sessionCounters)); } catch (e) { console.log("cube-timer: counter not saved: " + e); }
        return n;
    }
    function nextId() {
        var m = 0;
        for (var k = 0; k < sessions.length; k++) m = Math.max(m, sessions[k].id);
        return m + 1;
    }
    // Make the current session one that belongs to the current event
    // (the most recent one, or a new "Session 1" if the event has none yet).
    function ensureSession() {
        var idx = -1;
        var rid = lastSessionByEvent[currentEvent.id];
        var ridIdx = -1;
        for (var k = 0; k < sessions.length; k++) {
            if (sessions[k].event === currentEvent.id) {
                idx = k;                                  // newest of this event (fallback)
                if (sessions[k].id === rid) ridIdx = k;   // last-used one, if it still exists
            }
        }
        if (ridIdx >= 0) idx = ridIdx;
        if (idx < 0) {
            var all = sessions.slice();
            all.push({ id: nextId(), name: "Session " + nextSessionNumber(), event: currentEvent.id,
                       created: Date.now(), solves: [] });
            sessions = all;
            idx = all.length - 1;
        }
        currentSession = idx;
        rememberSession();
    }
    // Sessions of the current event, for the picker: [{index, name, count}].
    function listSessionsForEvent() {
        var r = [];
        for (var k = 0; k < sessions.length; k++) {
            if (sessions[k].event === currentEvent.id)
                r.push({ index: k, name: sessions[k].name, id: sessions[k].id, count: sessions[k].solves.length });
        }
        return r;
    }
    // "+" button: create the next session for this event and switch to it.
    function newSession() {
        if (phase !== "idle") return;
        var all = sessions.slice();
        all.push({ id: nextId(), name: "Session " + nextSessionNumber(),
                   event: currentEvent.id, created: Date.now(), solves: [] });
        sessions = all;
        currentSession = all.length - 1;
        showSolve();
        persist();
    }
    function switchSession(idx) {
        if (phase !== "idle") return;
        if (idx < 0 || idx >= sessions.length) return;
        currentSession = idx;
        showSolve();
        persist();
    }
    // Inline event drop-down under the event row (same pattern as the session menu).
    property bool eventMenuOpen: false
    function toggleEventMenu() {
        if (phase !== "idle") return;
        if (!eventMenuOpen) { sessionMenuOpen = false; openRowId = -1; renamingId = -1; }
        eventMenuOpen = !eventMenuOpen;
    }
    function closeEventMenu() { eventMenuOpen = false; }
    function pickEvent(i) {
        eventMenuOpen = false;
        if (i !== eventIndex) selectEvent(i);
    }
    // Inline session drop-down under the session row (no separate page).
    property bool sessionMenuOpen: false
    readonly property bool pullDownAllowed: hasSolves && !eventMenuOpen && !sessionMenuOpen
    readonly property var sessionMenuItems: listSessionsForEvent()
    // Discoverability peek: the current row nudges left on the first 5 menu opens only.
    property int peekCount: 0
    property int peekTick: 0
    function toggleSessionMenu() {
        if (phase !== "idle") return;
        if (!sessionMenuOpen && peekCount < 5) {
            peekCount += 1;
            try { setSetting("peekCount", peekCount); } catch (e) { console.log("cube-timer: peekCount not saved: " + e); }
            peekTick += 1;                          // every row listens; only the current one moves
        }
        if (!sessionMenuOpen) eventMenuOpen = false;
        sessionMenuOpen = !sessionMenuOpen;
    }
    function pickSession(idx) {
        sessionMenuOpen = false;
        openRowId = -1;
        renamingId = -1;
        switchSession(idx);
    }

    // Session row UI state: which row shows its actions, and which row is being renamed.
    property int openRowId: -1
    property int renamingId: -1
    function closeSessionMenu() {
        sessionMenuOpen = false;
        openRowId = -1;
        renamingId = -1;
    }
    function startRename(sid) {
        if (phase !== "idle") return;
        openRowId = -1;
        renamingId = sid;
        buzz.play();                                   // short haptic tick
    }
    // Save a new name (empty text cancels). The internal id and the session counter are untouched.
    function confirmRename(sid, txt) {
        renamingId = -1;
        var n = String(txt).trim();
        if (n.length === 0) return;
        var idx = -1;
        for (var k = 0; k < sessions.length; k++) if (sessions[k].id === sid) idx = k;
        if (idx < 0) return;
        var all = sessions.slice();
        var o = all[idx];
        all[idx] = { id: o.id, name: n, event: o.event, created: o.created, solves: o.solves };
        sessions = all;
        persist();
    }
    function cancelRename() {
        renamingId = -1;
    }
    // The inline rename field (set by the session row that is renaming); the rename blocker
    // lets presses inside it through.
    property var renameFieldItem: null

    // Delete a session (by id). Other sessions keep their names. If it was the current
    // one, switch to the most recent remaining session of this event, or create the next
    // numbered one if none is left.
    function deleteSessionById(sid) {
        if (phase !== "idle") return;
        openRowId = -1;
        var idx = -1;
        for (var k = 0; k < sessions.length; k++) if (sessions[k].id === sid) idx = k;
        if (idx < 0) return;
        if (renamingId === sid) renamingId = -1;
        var wasCurrent = (idx === currentSession);
        var all = sessions.slice();
        all.splice(idx, 1);
        var cur = (idx < currentSession) ? currentSession - 1 : currentSession;
        if (wasCurrent) {
            var found = -1;
            for (var j = all.length - 1; j >= 0; j--) if (all[j].event === currentEvent.id) { found = j; break; }
            if (found < 0) {
                all.push({ id: nextId(), name: "Session " + nextSessionNumber(), event: currentEvent.id,
                           created: Date.now(), solves: [] });
                found = all.length - 1;
            }
            cur = found;
        }
        sessions = all;
        currentSession = cur;
        showSolve();
        persist();
    }

    // Replace the solves of the current session and refresh the display.
    function writeSolves(newSolves) {
        var all = sessions.slice();
        var cur = all[currentSession];
        all[currentSession] = { id: cur.id, name: cur.name, event: cur.event,
                                created: cur.created, solves: newSolves };
        sessions = all;
        showSolve();
        persist();
    }
    function appendSolve(s) {
        writeSolves(sessionSolves.concat([s]));
    }

    // ---- solve state ----------------------------------------------------
    // ---- WCA scrambles -----------------------------------------------------
    // Generated natively (src/scramble: C++ port of csTimer, GPL-3.0) on a worker thread, through the
    // nativeScrambler property map: QML writes a JSON request, the reply arrives in .reply. Only the
    // latest request counts; a reply for an older one (event switched meanwhile) is ignored.
    property int scrReqSeq: 0
    property int scrCurrentReq: -1                   // request for the shown scramble (-1: none)
    readonly property bool scramblePending: scramble === "" && scrCurrentReq !== -1
    property string scrReply: nativeScrambler.reply
    onScrReplyChanged: {
        if (scrReply === "") return;
        var m = JSON.parse(scrReply);
        if (m.reqId !== scrCurrentReq) return;
        scrCurrentReq = -1;
        if (m.error) console.log("scramble error (" + m.event + "): " + m.error);
        else if (m.event === currentEvent.id && scramble === "") scramble = m.scramble;
    }
    function newScramble() {
        scramble = "";
        scrReqSeq += 1;
        scrCurrentReq = scrReqSeq;
        nativeScrambler.request = JSON.stringify({ reqId: scrReqSeq, event: currentEvent.id });
    }

    // Scramble history (in memory): one step back (prevScramble) and one step forward (fwdScramble).
    //   swipe left : forward. If a forward step exists (we came back), go to it; else generate a new one.
    //                Current becomes the previous.
    //   swipe right: back to prevScramble (if any). Current becomes the forward step.
    //   solve stop : the scramble just used becomes the previous; a new one is generated.
    //   event change clears both.
    property string prevScramble: ""
    property string fwdScramble: ""
    function swipeForward() {
        prevScramble = scramble;
        if (fwdScramble !== "") {
            scramble = fwdScramble;
            fwdScramble = "";
        } else {
            newScramble();
        }
    }
    function swipeBack() {
        if (prevScramble === "") return;
        fwdScramble = scramble;
        scramble = prevScramble;
        prevScramble = "";
        scrCurrentReq = -1;                          // a scramble still being generated is dropped
    }
    function scrambleAfterSolve() {
        prevScramble = scramble;
        fwdScramble = "";
        newScramble();
    }

    // Show the last solve of the current session (or 0.00) in the display labels.
    function showSolve() {
        closeSolvePopup();                            // the popup may show a solve that is gone now
        var s = lastSolve;
        if (!s) {
            timeText = "0.00";
            subText = "";
            return;
        }
        var pen = s.penalty || { plus2Count: 0, dnf: false };
        var added = 2000 * pen.plus2Count;
        if (pen.dnf) {                                // DNF overrides +2; raw time below
            timeText = "DNF";
            subText = "(" + fmt(s.time_ms) + ")";
        } else if (pen.plus2Count > 0) {              // "16.34" with "(+4)" below
            timeText = fmt(s.time_ms + added);
            subText = "(+" + added / 1000 + ")";
        } else {
            timeText = fmt(s.time_ms);
            subText = "";
        }
    }

    function selectEvent(i) {
        sessionMenuOpen = false;
        eventMenuOpen = false;
        if (phase === "running") runTimer.stop();     // abandon the current solve
        if (phase === "holding") holdTimer.stop();
        setTopMode("normal");
        phase = "idle";
        ready = false;
        setScrambleMode("normal");                   // before wipeAnim.stop(): onStopped only acts in "sweep"
        wipeAnim.stop();
        wipeShift.y = 0;
        eventIndex = i;
        prevScramble = "";                            // history is per event
        fwdScramble = "";
        ensureSession();                              // sessions are per event
        showSolve();
        newScramble();
        persist();
    }

    // Penalties on the last solve. Each call replaces the solve object.
    function setPenalty(plus2Count, dnf) {
        if (!canEditLast) return;
        var solves = sessionSolves.slice();
        var s = solves[solves.length - 1];
        solves[solves.length - 1] = { time_ms: s.time_ms,
                                      penalty: { plus2Count: plus2Count, dnf: dnf },
                                      event: s.event, scramble: s.scramble };
        writeSolves(solves);
    }
    function addPlus2() {
        if (!canEditLast) return;
        setPenalty(lastSolve.penalty.plus2Count + 1, lastSolve.penalty.dnf);
    }
    function setDnf() {
        if (!canEditLast) return;
        setPenalty(lastSolve.penalty.plus2Count, true);
    }
    function clearPenalty() {
        if (!canEditLast) return;
        setPenalty(0, false);
    }

    // Removes the last solve of the current session; the display falls back to the one before it.
    function deleteSolve() {
        if (!canEditLast) return;
        writeSolves(sessionSolves.slice(0, sessionSolves.length - 1));
    }

    function beginHold() {
        sessionMenuOpen = false;
        eventMenuOpen = false;
        setScrambleMode("dim");
        setTopMode("dim");
        holdStart = Date.now();
        setHoldDragSlop(true);
        phase = "holding";
        ready = false;
        timeText = "0.00";
        subText = "";
        holdTimer.start();
    }

    function cancelHold() {
        setHoldDragSlop(false);
        holdTimer.stop();
        setScrambleMode("normal");
        setTopMode("normal");
        phase = "idle";
        ready = false;
        showSolve();                                  // restore the previous solve display
    }

    function startSolve() {
        setHoldDragSlop(false);
        holdTimer.stop();
        setScrambleMode("sweep");                      // dimmed scramble stays; the wipe moves over it
        setTopMode("hidden");
        phase = "running";
        wipeAnim.to = -(scrambleBox.height + wipeMask.bandH);
        wipeAnim.start();
        ready = false;
        startMs = Date.now();
        timeText = "0.00";
        subText = "";
        runTimer.start();
    }

    // A single tap (on release) stops the solve and stores it in the current session.
    function stopSolve(now) {
        runTimer.stop();
        noFade = true;                                 // no fade-in of scramble/stats on stop
        setScrambleMode("normal");                     // scramble back at once (before wipeAnim.stop)
        setTopMode("normal");
        wipeAnim.stop();
        wipeShift.y = 0;
        var s = { time_ms: now - startMs, penalty: { plus2Count: 0, dnf: false },
                  event: currentEvent.id, scramble: scramble };
        lastStopMs = now;
        phase = "idle";
        ready = false;
        appendSolve(s);
        scrambleAfterSolve();
        noFade = false;
        if (lastIsPb) { pbPop.restart(); confetti.burst(); }   // new PB: celebrate (only here)
    }

    // ---- solve detail popup (tap a history column) ----------------------
    property bool popupOpen: false
    property var popupSolve: null                    // solve object shown in the popup
    property int popupN: 0                           // its 1-based index in the session
    property int popupIndex: -1                      // 0-based index (used by the card's delete)
    function openSolvePopup(idx) {
        var ss = sessionSolves;
        if (idx < 0 || idx >= ss.length) return;
        popupSolve = ss[idx];
        popupN = idx + 1;
        popupIndex = idx;
        popupOpen = true;
    }
    // Deletes one solve of the current session (any position). writeSolves refreshes the display
    // (last solve fallback, stats, rolling averages), persists, and closes the popup.
    function deleteSolveAt(idx) {
        var ss = sessionSolves;
        if (idx < 0 || idx >= ss.length) return;
        writeSolves(ss.slice(0, idx).concat(ss.slice(idx + 1)));
    }
    function closeSolvePopup() { popupOpen = false; popupHoldTimer.stop(); }
    function eventName(id) {
        for (var i = 0; i < events.length; i++) if (events[i].id === id) return events[i].name;
        return id;
    }
    function scrambleOf(s) { return (s && s.scramble) ? s.scramble : "(no scramble saved)"; }
    // Popup time: penalised total (or "DNF"), plus a smaller suffix: "(+N)" for +2s, "(raw)" for DNF.
    function popupMain(s) {
        if (!s) return "";
        if (s.penalty && s.penalty.dnf) return "DNF";
        return Stats.fmtMs(s.time_ms + 2000 * ((s.penalty && s.penalty.plus2Count) || 0));
    }
    function popupSuffix(s) {
        if (!s || !s.penalty) return "";
        if (s.penalty.dnf) return "(" + Stats.fmtMs(s.time_ms) + ")";
        if (s.penalty.plus2Count > 0) return "(+" + (2 * s.penalty.plus2Count) + ")";
        return "";
    }

    // Single-finger touch handling (the touch MouseArea). Presses during a pinch are ignored.
    // While running, the full-page stop layer takes every press, so this is never reached then.
    // Presses within cooldownMs of a stop are ignored.
    function onPress() {
        if (pinching) return;
        if (Date.now() - lastStopMs < cooldownMs) return;
        if (phase === "idle") beginHold();
    }

    function onRelease() {
        var tableTap = tableTapPending && !histFlick.moving && !histFlick.flicking;
        tableTapPending = false;
        if (pinching) return;
        if (tableTap) { statsExpanded = false; return; } // tap on labels / blank table area
        if (phase !== "holding") return;
        holdTimer.stop();
        if (Date.now() - holdStart >= readyDelayMs) {
            startSolve();                             // a hold leaves the table expanded
        } else {
            cancelHold();
        }
    }

    function onTouchCanceled() {
        tableTapPending = false;
        if (phase === "holding") cancelHold();
    }

    // Presses inside the stats block (x, y in touch-area coordinates) never start a hold:
    // collapsed -> expand; expanded -> labels column collapses, table rect is excluded
    // (no solve). Returns "consume" when the press is handled here, "none" otherwise.
    // A press while running is left to onPress (it stops the solve).
    function statsPress(x, y) {
        tableTapPending = false;
        if (pinching || phase === "running") return "none";
        var p = touch.mapToItem(statsBox, x, y);
        var inside = p.x >= 0 && p.y >= 0 && p.x <= statsBox.width && p.y <= statsBox.height;
        if (!inside) return "none";     // outside the table: no collapse (only the labels column collapses)
        if (!statsExpanded) {
            statsExpanded = true;
        } else {
            // Labels column and the best column: a tap collapses on release. Anything else in the
            // table is never a solve: history columns open their popup from their own MouseArea
            // (inside the Flickable).
            var lp = touch.mapToItem(statsLabels, x, y);
            var bp = touch.mapToItem(bestCol, x, y);
            if ((lp.x >= 0 && lp.x <= statsLabels.width) || (bp.x >= 0 && bp.x <= bestCol.width)) tableTapPending = true;
            tablePressX = x;
            tablePressY = y;
        }
        return "consume";
    }

    Component.onCompleted: {
        loadSettings();
        if (db) db.transaction(function (tx) { tx.executeSql("DROP TABLE IF EXISTS scramble_pool"); });   // old JS-era cache
        newScramble();
        showSolve();
    }

    Timer {
        id: holdTimer
        interval: 16
        repeat: true
        onTriggered: {
            if (!page.ready && Date.now() - page.holdStart >= page.readyDelayMs) {
                page.ready = true;
                page.setHoldDragSlop(false);   // green: preventStealing takes over
            }
        }
    }

    Timer {
        id: runTimer
        interval: 10
        repeat: true
        onTriggered: page.timeText = page.fmt(Date.now() - page.startMs)
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentHeight: height
        // No pull-down menu without solves: stop at the bounds so nothing overshoots.
        boundsBehavior: page.pullDownAllowed ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds

        // Pull-down: solve actions on the last solve of the current session.
        PullDownMenu {
            // Off while the event or session drop-down is open (drags there must not pull the menu).
            enabled: page.pullDownAllowed
            visible: page.hasSolves
            MenuItem {

                font.family: page.appFont; font.letterSpacing: Theme.fontSizeMedium * page.trackK
                text: "OK"
                enabled: page.canEditLast
                onClicked: page.clearPenalty()
            }
            MenuItem {

                font.family: page.appFont; font.letterSpacing: Theme.fontSizeMedium * page.trackK
                text: "Delete"
                enabled: page.canEditLast
                onClicked: page.deleteSolve()
            }
            MenuItem {

                font.family: page.appFont; font.letterSpacing: Theme.fontSizeMedium * page.trackK
                text: "DNF"
                enabled: page.canEditLast
                onClicked: page.setDnf()
            }
            MenuItem {

                font.family: page.appFont; font.letterSpacing: Theme.fontSizeMedium * page.trackK
                text: "+2"
                enabled: page.canEditLast
                onClicked: page.addPlus2()
            }
        }

        // Touch surface: only the region BELOW the scramble text down to the
        // page bottom. Header row, session row and scramble stay untouched, so the
        // PullDownMenu drags there still reach the flickable. The PinchArea
        // wraps the MouseArea so two-finger zoom works; single-finger
        // hold/tap is handled by the MouseArea.
        PinchArea {
            id: pinchArea
            anchors.top: scrambleBox.bottom
            anchors.topMargin: Theme.paddingLarge
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            pinch.target: null

            onPinchStarted: {
                page.pinching = true;
                page.pinchBaseSize = page.timeSize;
                if (page.phase === "holding") page.cancelHold();
            }
            onPinchUpdated: {
                if (page.phase === "running") return;   // no zoom mid-solve
                page.timeSize = page.clampSize(page.pinchBaseSize * pinch.scale);
            }
            onPinchFinished: page.pinching = false;   // zoom is not persisted

            MouseArea {
                id: touch
                anchors.fill: parent
                // Keep the grab only once the hold is ready (green) or the solve is running:
                // before ready, a drag may still go to the page flickable (PullDownMenu) and
                // cancel the hold; after ready, finger drift can no longer cancel it.
                // (Red phase drift tolerance: see setHoldDragSlop.)
                preventStealing: (page.phase === "holding" && page.ready) || page.phase === "running"
                onPressed: {
                    if (page.statsPress(mouse.x, mouse.y) === "consume") return;
                    page.onPress();
                }
                onPositionChanged: {
                    if (!page.tableTapPending) return;
                    var slop = Theme.startDragDistance || 15;
                    if (Math.abs(mouse.x - page.tablePressX) >= slop || Math.abs(mouse.y - page.tablePressY) >= slop) {
                        page.tableTapPending = false;   // a drag: scrolls, never collapses
                    }
                }
                onReleased: page.onRelease()
                onCanceled: page.onTouchCanceled()
            }
        }

        // Faint highlight of the touch region while holding. Tint follows the hold state:
        // soft red while not ready, soft green when ready (120 ms colour change). Top edge is a
        // short soft fade (transparent -> tint over ~2 x paddingLarge), then solid.
        Rectangle {
            id: holdHighlight
            anchors.fill: pinchArea
            readonly property real fadeBand: Theme.paddingLarge * 3.2   // ~1.6x the old 2 x paddingLarge (~75 px -> ~120 px)
            // Alpha is part of the tint (red ~0.10, green ~0.12), so it animates with the colour.
            property color tint: page.ready ? Qt.rgba(page.readyColor.r, page.readyColor.g, page.readyColor.b, 0.12)
                                            : Qt.rgba(page.notReadyColor.r, page.notReadyColor.g, page.notReadyColor.b, 0.10)
            Behavior on tint { ColorAnimation { duration: 120 } }
            gradient: Gradient {
                GradientStop { position: 0; color: Qt.rgba(holdHighlight.tint.r, holdHighlight.tint.g, holdHighlight.tint.b, 0) }
                GradientStop { position: Math.min(0.5, holdHighlight.fadeBand / Math.max(1, holdHighlight.height)); color: holdHighlight.tint }
                GradientStop { position: 1; color: holdHighlight.tint }
            }
            opacity: page.phase === "holding" ? 1 : 0
            Behavior on opacity { FadeAnimation { duration: 120 } }
        }

        // Top area. Row 1: event selector, centred. Row 2: session name centred
        // (tap = picker), with a round "+" button at the right edge (new session).
        Item {
            id: topRow
            anchors.top: parent.top
            anchors.topMargin: Theme.paddingLarge * 2 + Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            height: eventButton.height + sessionRowItem.height

            Item {
                id: eventButton
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: eventText.implicitHeight + Theme.paddingLarge
                opacity: 1
                Behavior on opacity { NumberAnimation { duration: 150 } }
                // Half-width of name + arrow, measured from the page centre (the name is centred).
                // Highlight and tap area use this same width, so both cover the arrow and the
                // padding is even around the name.
                readonly property real halfW: eventText.implicitWidth / 2 + Theme.paddingLarge + eventArrow.implicitWidth + Theme.paddingMedium

                // Pressed feedback: translucent highlight over name + arrow.
                Rectangle {
                    anchors.centerIn: parent
                    width: 2 * eventButton.halfW
                    height: parent.height + 2 * Theme.paddingMedium
                    radius: Theme.paddingSmall
                    color: page.pressFill
                    opacity: eventArea.pressed ? 1 : 0
                }

                // Event name: exactly centred on the page.
                Label {
                    id: eventText
                    anchors.centerIn: parent
                    text: page.currentEvent.name
                    // Main title: bold, primary colour; accent while pressed; dimmed while a solve runs.
                    color: page.phase === "idle"
                           ? (eventArea.pressed ? page.accentColor : page.textColor)
                           : page.lavenderColor
                    font.pixelSize: Theme.fontSizeExtraLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraLarge) * page.trackK
                    font.bold: true
                }

                // Down arrow attached to the right of the name, same gap as the session "+".
                Label {
                    id: eventArrow
                    anchors.left: eventText.right
                    anchors.leftMargin: Theme.paddingLarge
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u25BE"
                    color: page.lavenderColor
                    font.pixelSize: Theme.fontSizeExtraLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraLarge) * page.trackK
                }

                // One tap target for name + arrow: toggles the event drop-down.
                MouseArea {
                    id: eventArea
                    anchors.centerIn: parent
                    width: 2 * eventButton.halfW
                    height: parent.height + 2 * Theme.paddingMedium
                    enabled: page.phase === "idle"
                    onClicked: page.toggleEventMenu()
                }
            }

            Item {
                id: sessionRowItem
                anchors.top: eventButton.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: sessionLabel.implicitHeight + Theme.paddingSmall
                opacity: 1
                Behavior on opacity { NumberAnimation { duration: 150 } }

                // Name and "+" as one group, centred on the row; "+" sits right next to the name.
                Item {
                    anchors.fill: parent

                    BackgroundItem {
                        id: sessionButton
                        anchors.centerIn: parent
                        width: sessionLabel.implicitWidth + 2 * Theme.paddingMedium
                        height: sessionLabel.implicitHeight + 2 * Theme.paddingMedium
                        enabled: page.phase === "idle"
                        onClicked: page.toggleSessionMenu()

                        Label {
                            id: sessionLabel
                            anchors.centerIn: parent
                            text: page.sessionName
                            // Soft secondary when idle; accent while pressed.
                            color: sessionButton.down ? page.accentColor : page.lavenderColor
                            font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK
                        }
                    }

                    // Round "+" button (new session), circular outline in highlight colour.
                    Item {
                        id: addSessionButton
                        anchors.left: sessionButton.right
                        anchors.leftMargin: Theme.paddingLarge - Theme.paddingMedium
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.fontSizeLarge * 1.1
                        height: Theme.fontSizeLarge * 1.1
                        opacity: addSessionArea.enabled ? 1 : 0.4

                        Rectangle {
                            anchors.fill: parent
                            radius: width / 2
                            color: "transparent"
                            border.width: 2
                            border.color: addSessionArea.pressed ? page.accentColor : page.lavenderColor

                            // pressed state: translucent fill
                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: page.pressFill
                                opacity: addSessionArea.pressed ? 1 : 0
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            text: "+"
                            font.pixelSize: Theme.fontSizeMedium; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeMedium) * page.trackK
                            color: addSessionArea.pressed ? page.accentColor : page.lavenderColor
                        }

                        // Tap area larger than the visible circle.
                        MouseArea {
                            id: addSessionArea
                            anchors.fill: parent
                            anchors.margins: -Theme.paddingMedium
                            enabled: page.phase === "idle"
                            onClicked: page.newSession()
                        }
                    }
                }
            }
        }

        Item {
            id: scrambleBox
            anchors.top: topRow.bottom
            anchors.topMargin: Theme.paddingLarge
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: page.scrambleSideMargin
            anchors.rightMargin: page.scrambleSideMargin
            height: scrambleText.height
            clip: true
            // Dimmed while holding; the start wipe below removes it. Layout stays fixed.
            opacity: 1                       // box stays opaque; text dims via setScrambleMode()

            // Clipping wrapper: height shrinks from the bottom edge upward; the text inside
            // keeps its position (anchored to the top), so nothing moves or reflows.
            Item {
                id: scrambleWipe
                anchors.top: parent.top
                width: parent.width
                height: scrambleBox.height
                clip: true
                // Start wipe: the transparency mask wipeMask, applied only while sweeping.
                layer.enabled: page.scrambleMode === "sweep"
                layer.effect: OpacityMask { maskSource: wipeMask }

            ScrambleGrid {
                id: scrambleText
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                text: page.scramble !== "" ? page.scramble : (page.scramblePending ? "Generating scramble\u2026" : "")
                eventId: page.currentEvent.id
                maxWidth: page.scrambleWidth
                maxHeight: page.scrambleMaxHeight
                fontFamily: page.appFont
                ink: page.copyFlash ? page.accentColor : page.dimColor
                transform: Translate { id: scrambleShift; x: 0 }
            }
            }

            // Start-wipe mask (used by scrambleWipe's OpacityMask, never drawn itself). Opaque
            // above a soft band; the band fades from opaque (top) to transparent (bottom), and
            // everything below it is transparent. The band starts just under the box (all
            // visible) and moves up by (box height + band) over 180 ms (wipeShift, OutCubic), so
            // the transparent part eats the scramble from the bottom up.
            Item {
                id: wipeMask
                readonly property real bandH: 120
                visible: false
                anchors.top: parent.top
                width: parent.width
                height: scrambleBox.height
                Item {
                    width: parent.width
                    height: wipeMask.bandH
                    y: scrambleBox.height
                    transform: Translate { id: wipeShift; y: 0 }
                    Rectangle {                          // solid part above the band
                        width: parent.width
                        height: page.height
                        y: -height
                        color: "#ffffffff"
                    }
                    Rectangle {                          // soft band: opaque top -> transparent bottom
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#ffffffff" }
                            GradientStop { position: 1; color: "#00ffffff" }
                        }
                    }
                }
            }

            // Swipe LEFT = forward, RIGHT = back to the previous scramble (idle only).
            // Direction lock lives in HSwipeArea.
            readonly property real swipeThreshold: Theme.itemSizeSmall

            // Slide out to the left, step forward, slide in from the right.
            SequentialAnimation {
                id: skipLeft
                ParallelAnimation {
                    NumberAnimation { target: scrambleShift; property: "x"; to: -scrambleBox.width; duration: 180; easing.type: Easing.InQuad }
                    NumberAnimation { target: scrambleText; property: "opacity"; to: 0; duration: 180 }
                }
                ScriptAction {
                    script: {
                        page.swipeForward();
                        scrambleShift.x = scrambleBox.width;
                    }
                }
                ParallelAnimation {
                    NumberAnimation { target: scrambleShift; property: "x"; to: 0; duration: 180; easing.type: Easing.OutQuad }
                    NumberAnimation { target: scrambleText; property: "opacity"; to: 1; duration: 180 }
                }
            }
            // Slide out to the right, step back, previous slides in from the left.
            SequentialAnimation {
                id: skipRight
                ParallelAnimation {
                    NumberAnimation { target: scrambleShift; property: "x"; to: scrambleBox.width; duration: 180; easing.type: Easing.InQuad }
                    NumberAnimation { target: scrambleText; property: "opacity"; to: 0; duration: 180 }
                }
                ScriptAction {
                    script: {
                        page.swipeBack();
                        scrambleShift.x = -scrambleBox.width;
                    }
                }
                ParallelAnimation {
                    NumberAnimation { target: scrambleShift; property: "x"; to: 0; duration: 180; easing.type: Easing.OutQuad }
                    NumberAnimation { target: scrambleText; property: "opacity"; to: 1; duration: 180 }
                }
            }
            // Small drag, or no previous to go back to: back to the original place (bounce).
            NumberAnimation { id: snapAnim; target: scrambleShift; property: "x"; to: 0; duration: 150; easing.type: Easing.OutQuad }

            HSwipeArea {
                id: scrambleSwipe
                anchors.fill: parent
                flickable: flick
                enabled: page.phase === "idle" && !skipLeft.running && !skipRight.running && !page.pinching
                holdMs: page.copyHoldMs
                onLongPress: page.copyScramble(page.scramble)
                onDragStart: {
                    snapAnim.stop();
                }
                onDragMove: {
                    if (dx < 0) scrambleShift.x = Math.max(-scrambleBox.width, dx);
                    else if (page.prevScramble !== "") scrambleShift.x = Math.min(scrambleBox.width, dx);
                    else scrambleShift.x = dx * 0.3;                      // nothing to go back to: rubber band
                }
                onDragEnd: {
                    if (dx <= -scrambleBox.swipeThreshold) skipLeft.start();
                    else if (dx >= scrambleBox.swipeThreshold && page.prevScramble !== "") skipRight.start();
                    else snapAnim.start();
                }
            }
        }

        // Width probes (same fonts as the cells): the columns are only as wide as their widest
        // text (see the page-level statsLabelW / statsValueW / histColW properties).
        Text { id: pl1; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: "time" }
        Text { id: pl2; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: "ao5" }
        Text { id: pl3; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: "ao12" }
        Text { id: pl4; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: "ao100" }
        Text { id: pvT;   visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: page.latestText }
        Text { id: pv5;   visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: page.stats.ao5 }
        Text { id: pv12;  visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: page.stats.ao12 }
        Text { id: pv100; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: page.stats.ao100 }
        Text { id: pHist; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; text: Stats.widthProbeText(page.history) }
        Text { id: pBest; visible: false; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; font.bold: true; text: Stats.widthProbeText([page.bests]) }
        Text { id: pBestHead; visible: false; font.pixelSize: Theme.fontSizeTiny; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeTiny) * page.trackK; font.bold: true; text: "best" }
        // Drop-down name probes: the widest event / session name sets the minimum list width.
        Text { id: eventNameProbe;   visible: false; font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK; text: page.widestName(page.events) }
        Text { id: sessionNameProbe; visible: false; font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK; text: page.widestName(page.sessionMenuItems) }

        // A faint dimmer under the bottom stats so the small numbers stay readable on bright
        // ambiences. Fades in from nothing at its top edge; follows the stats.
        Rectangle {
            z: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: page.height * 0.25
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.rgba(Theme.highlightDimmerColor, 0) }
                GradientStop { position: 1; color: Theme.rgba(Theme.highlightDimmerColor, 0.3) }
            }
            opacity: statsBox.opacity
        }

        // Stats block, bottom-right. Collapsed: [labels | time, ao5, ao12, ao100 of the latest solve].
        // Tap it to expand leftward: [labels | per-solve columns, newest at the right | session bests].
        // Opacity follows the scramble (dim while holding, hidden while running). Presses are
        // routed by the touch area (page.statsPress); the table rect never starts a solve.
        Item {
            id: statsBox
            z: 2
            anchors.right: parent.right
            anchors.rightMargin: Theme.horizontalPageMargin
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.paddingLarge * 2
            width: page.statsCollapsedW + (parent.width - 2 * Theme.horizontalPageMargin - page.statsCollapsedW) * page.expandT
            height: page.histHeaderH * page.expandT + 4 * page.statsRowH
            opacity: page.idleOpacity
            Behavior on opacity { enabled: !page.noFade; NumberAnimation { duration: 150 } }

            // Row labels, left edge of the block (both states). Empty header slot when collapsed.
            Column {
                id: statsLabels
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                width: page.statsLabelW
                Item { width: parent.width; height: page.histHeaderH * page.expandT }
                Label { width: parent.width; height: page.statsRowH; text: "time"; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignLeft }
                Label { width: parent.width; height: page.statsRowH; text: "ao5"; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignLeft }
                Label { width: parent.width; height: page.statsRowH; text: "ao12"; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignLeft }
                Label { width: parent.width; height: page.statsRowH; text: "ao100"; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignLeft }
            }

            // Collapsed values: latest solve and the three averages (right-aligned).
            Column {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: page.statsValueW
                opacity: 1 - page.expandT
                visible: opacity > 0
                Label { width: parent.width; height: page.statsRowH; text: page.latestText; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.stats.ao5; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.stats.ao12; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.stats.ao100; color: page.lavenderColor; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
            }

            // Expanded history: one column per solve of the session, oldest at the left.
            // Scrolls horizontally; starts at the newest column (right edge). The outer
            // flickable is locked while this one moves, so a horizontal drag cannot pull down.
            // Expanded only: session bests, pinned at the right edge in the accent colour.
            Column {
                id: bestCol
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: page.bestColW
                visible: page.expandT > 0
                opacity: page.expandT
                Label { width: parent.width; height: page.histHeaderH; text: "best"; color: page.bestColor; font.bold: true; font.pixelSize: Theme.fontSizeTiny; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeTiny) * page.trackK; horizontalAlignment: Text.AlignRight }
                Label { width: parent.width; height: page.statsRowH; text: page.bests.time; color: page.bestColor; font.bold: true; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.bests.ao5; color: page.bestColor; font.bold: true; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.bests.ao12; color: page.bestColor; font.bold: true; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                Label { width: parent.width; height: page.statsRowH; text: page.bests.ao100; color: page.bestColor; font.bold: true; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
            }

            Flickable {
                id: histFlick
                anchors.left: statsLabels.right
                anchors.leftMargin: Theme.paddingLarge
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: bestCol.left
                anchors.rightMargin: 0
                visible: page.expandT > 0
                opacity: page.expandT
                clip: true
                contentWidth: Math.max(width, histRow.width)
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick

                // Auto-follow the newest column only if the table was already there (or within half a
                // column of it). Otherwise the same columns stay in view when solves are added or
                // changed. Collapsing and re-expanding resets to the newest column.
                // followNewest / savedX change ONLY from user scrolling (movement end). Programmatic
                // changes (Repeater rebuild after a solve, bounds clamping) must not flip them, or a
                // table scrolled to older solves would jump to the newest (or vice versa).
                property bool followNewest: true
                property real savedX: 0
                function settle() {
                    if (followNewest) page.scrollHistToNewest();
                    else contentX = Math.min(savedX, Math.max(0, contentWidth - width));
                }
                onContentWidthChanged: settleTimer.restart()
                onWidthChanged: settleTimer.restart()
                Timer { id: settleTimer; interval: 0; onTriggered: histFlick.settle() }
                onMovementStarted: flick.interactive = false
                onMovementEnded: {
                    flick.interactive = true;
                    followNewest = contentX >= Math.max(0, contentWidth - width) - page.histColW / 2;
                    savedX = contentX;
                }

                Row {
                    id: histRow
                    anchors.top: parent.top
                    anchors.right: parent.right
                    height: page.histHeaderH + 4 * page.statsRowH
                    Repeater {
                        model: page.expandT > 0 ? (page.history.length > 0 ? page.history : page.emptyHist) : []
                        Item {
                            width: page.histColW
                            height: histRow.height
                            Column {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                width: page.histColW
                                Label { width: parent.width - Theme.paddingMedium; height: page.histHeaderH; text: "#" + modelData.n; color: page.lavenderColor; font.pixelSize: Theme.fontSizeTiny; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeTiny) * page.trackK; horizontalAlignment: Text.AlignRight }
                                Label { width: parent.width - Theme.paddingMedium; height: page.statsRowH; text: modelData.time; color: (page.history.length > 0 && index === page.bestSingleIdx) ? page.bestColor : page.lavenderColor; font.bold: page.history.length > 0 && index === page.bestSingleIdx; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                                Label { width: parent.width - Theme.paddingMedium; height: page.statsRowH; text: modelData.ao5; color: (page.history.length > 0 && index === page.bestAoIdx.ao5) ? page.bestColor : page.lavenderColor; font.bold: page.history.length > 0 && index === page.bestAoIdx.ao5; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                                Label { width: parent.width - Theme.paddingMedium; height: page.statsRowH; text: modelData.ao12; color: (page.history.length > 0 && index === page.bestAoIdx.ao12) ? page.bestColor : page.lavenderColor; font.bold: page.history.length > 0 && index === page.bestAoIdx.ao12; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                                Label { width: parent.width - Theme.paddingMedium; height: page.statsRowH; text: modelData.ao100; color: (page.history.length > 0 && index === page.bestAoIdx.ao100) ? page.bestColor : page.lavenderColor; font.bold: page.history.length > 0 && index === page.bestAoIdx.ao100; font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK; horizontalAlignment: Text.AlignRight; elide: Text.ElideLeft }
                            }
                            // Tap on a history column, inside the Flickable content. Qt emits clicked
                            // only when the Flickable did not take the press over as a drag.
                            // Placeholder column (no solves) has no popup.
                            MouseArea {
                                anchors.fill: parent
                                enabled: page.history.length > 0
                                preventStealing: false
                                propagateComposedEvents: false
                                onClicked: {
                                    if (histFlick.moving || histFlick.flicking) return;
                                    page.openSolvePopup(index);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Hidden probe (QtQuick 2.0 has no TextMetrics type on this device).
        // Only its implicitWidth is used, to size the time font.
        Text {
            id: sizeProbe
            visible: false
            font.pixelSize: 100; font.family: page.appFont; font.letterSpacing: (100) * page.trackBigK
            font.bold: true
            text: "9:59.99"
        }

        // Time text: centered in the whole page, independent of scramble height.
        // Declared after the scramble so it draws on top of it.
        // Region from the bottom of the session row to the page bottom. The time is centred
        // here, so it stays at a fixed position regardless of scramble height or event.
        Item {
            id: timeRegion
            anchors.top: topRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }

        // Time text: bold, centred in timeRegion.
        Label {
            id: timeLabel
            anchors.centerIn: timeRegion
            width: page.width - 2 * Theme.paddingMedium
            horizontalAlignment: Text.AlignHCenter
            // Long times (1:00:00.00, +2, DNF) shrink to the width instead of overflowing.
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeTiny
            text: page.timeText
            font.pixelSize: page.timeSize; font.family: page.appFont; font.letterSpacing: (page.timeSize) * page.trackBigK
            font.bold: true
            color: page.phase === "holding"
                   ? (page.ready ? page.readyColor : page.notReadyColor)
                   : page.textColor
        }

        // Penalty line under the big time: DNF raw time or +2 total.
        Label {
            id: penaltyLabel
            anchors.top: timeLabel.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: page.width - 2 * Theme.paddingMedium
            horizontalAlignment: Text.AlignHCenter
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Theme.fontSizeTiny
            text: page.subText
            color: page.dimColor
            font.pixelSize: page.penaltySize; font.family: page.appFont; font.letterSpacing: (page.penaltySize) * page.trackK
        }

        // "PB" under the time (under the penalty line when there is one) while idle, when the
        // last solve is a new best single of the session. Same accent colour as the best column.
        Label {
            id: pbLabel
            z: 4                                   // above the confetti (z 3), stays readable
            anchors.top: page.subText !== "" ? penaltyLabel.bottom : timeLabel.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            visible: page.lastIsPb && page.phase === "idle"
            text: "PB"
            color: page.bestColor
            font.bold: true
            font.pixelSize: page.penaltySize; font.family: page.appFont; font.letterSpacing: (page.penaltySize) * page.trackK
        }

        // PB celebration (only right after a solve that is a new PB, see stopSolve): a confetti
        // burst from the PB label plus a pop of the label. One animated value (burstT 0 -> 1)
        // drives every piece; each piece gets a random angle / distance / spin per burst.
        Item {
            id: confetti
            z: 3
            x: pbLabel.x + pbLabel.width / 2
            y: pbLabel.y + pbLabel.height / 2
            width: 0; height: 0
            property real burstT: 1
            visible: burstT < 1
            readonly property var palette: [page.bestColor, page.readyColor, "#ffd36e", "#ff9ad5", "#ffffff"]
            function burst() {
                for (var i = 0; i < pieces.count; i++) {
                    var p = pieces.itemAt(i);
                    if (!p) continue;
                    p.a = -Math.PI * (0.08 + 0.84 * Math.random());          // mostly upwards fan
                    p.d = Theme.itemSizeLarge * (1.4 + 1.6 * Math.random());
                    p.spin = (Math.random() < 0.5 ? -1 : 1) * (180 + 360 * Math.random());
                    p.c = palette[i % palette.length];
                }
                burstAnim.restart();
            }
            Repeater {
                id: pieces
                model: 18
                Rectangle {
                    property real a: 0
                    property real d: 0
                    property real spin: 0
                    property color c: "white"
                    readonly property real t: confetti.burstT
                    width: Theme.paddingMedium * (index % 3 === 0 ? 1.4 : 1); height: Theme.paddingSmall * 1.6
                    radius: height / 3
                    color: c
                    x: Math.cos(a) * d * t - width / 2
                    y: Math.sin(a) * d * t + Theme.itemSizeLarge * 1.2 * t * t - height / 2   // + gravity
                    rotation: spin * t
                    opacity: 1 - t * t
                }
            }
            NumberAnimation { id: burstAnim; target: confetti; property: "burstT"; from: 0; to: 1; duration: 900; easing.type: Easing.OutQuad }
        }
        SequentialAnimation {
            id: pbPop
            NumberAnimation { target: pbLabel; property: "scale"; from: 0.4; to: 1.25; duration: 220; easing.type: Easing.OutBack }
            NumberAnimation { target: pbLabel; property: "scale"; to: 1; duration: 200; easing.type: Easing.InOutQuad }
        }

        // Event drop-down: opens right under the event row. Full-width rows, centred names,
        // current event in accentColor, max 40% of the page then scrolls. No swipe actions.
        // Tap a row = select that event and close; tap outside = close.
        Item {
            id: eventMenu
            visible: page.eventMenuOpen
            z: 10
            anchors.top: topRow.top
            anchors.topMargin: eventButton.height
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                anchors.fill: parent
                color: "#000000"
                opacity: 0.5
            }
            MouseArea {
                anchors.fill: parent
                onClicked: page.closeEventMenu()
            }

            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: Theme.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(page.width - 2 * Theme.horizontalPageMargin,
                                Math.max(page.width * 0.7, eventNameProbe.implicitWidth + 2 * Theme.paddingLarge))
                height: eventList.height + 2 * Theme.paddingSmall
                radius: page.cornerRadius
                color: page.panelColor
                clip: true

                ListView {
                    id: eventList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: Theme.paddingSmall
                    height: Math.min(count * Theme.itemSizeSmall, page.height * 0.4)
                    clip: true
                    model: page.events
                    VerticalScrollDecorator { flickable: eventList }

                    delegate: BackgroundItem {
                        id: eventDelegate
                        width: eventList.width
                        height: Theme.itemSizeSmall
                        // Press tint: inset rounded rect (same as the session rows), so it never pokes
                        // out of the list's rounded corners. The default full-row highlight is off.
                        highlightedColor: "transparent"
                        onClicked: page.pickEvent(index)
                        // Children of a BackgroundItem are reparented into its contentItem, so
                        // `parent` is not the delegate: read `down` through the id.
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            radius: Math.max(0, page.cornerRadius - Theme.paddingSmall)
                            color: page.pressFill
                            opacity: eventDelegate.down ? 1 : 0
                        }

                        Label {
                            anchors.centerIn: parent
                            text: modelData.name
                            color: index === page.eventIndex ? page.accentColor : page.textColor
                            font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK
                        }
                    }
                }
            }
        }

        // Session drop-down: opens right under the session row and overlays the
        // scramble/touch area only. Each session is a full-width row (ListView, scrolls
        // when taller than 40% of the page). Left swipe on a row reveals Delete; long press renames;
        // one row open at a time; tap a row to pick it, tap outside to close.
        Item {
            id: sessionMenu
            visible: page.sessionMenuOpen
            z: 10
            anchors.top: topRow.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            // Dim backdrop: a tap outside the list closes the menu.
            Rectangle {
                anchors.fill: parent
                color: "#000000"
                opacity: 0.5
            }
            MouseArea {
                anchors.fill: parent
                onClicked: page.closeSessionMenu()
            }

            Rectangle {
                id: sessionMenuBox
                anchors.top: parent.top
                anchors.topMargin: Theme.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(page.width - 2 * Theme.horizontalPageMargin,
                                Math.max(page.width * 0.7, sessionNameProbe.implicitWidth + 2 * Theme.paddingLarge))
                height: menuList.height + 2 * Theme.paddingSmall
                radius: page.cornerRadius
                color: page.panelColor
                clip: true

                ListView {
                    id: menuList
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: Theme.paddingSmall
                    height: Math.min(count * Theme.itemSizeSmall, page.height * 0.4)
                    clip: true
                    model: page.sessionMenuItems
                    VerticalScrollDecorator { flickable: menuList }

                    delegate: Item {
                        id: menuRow
                        width: menuList.width
                        height: Theme.itemSizeSmall
                        clip: true
                        readonly property int rowId: modelData.id
                        // One compact action button: icon width + paddingMedium on each side.
                        readonly property real actionW: Theme.iconSizeMedium + 2 * Theme.paddingMedium
                        readonly property bool isOpen: page.openRowId === rowId
                        readonly property bool isRenaming: page.renamingId === rowId
                        readonly property real openX: -actionW
                        property real baseX: 0
                        property real dragX: 0
                        property bool dragging: false
                        property real peekOffset: 0
                        readonly property bool isCurrentRow: modelData.index === page.currentSession

                        Connections {
                            target: page
                            onPeekTickChanged: if (menuRow.isCurrentRow) peekAnim.restart()
                        }
                        // Quick nudge left (reveals a sliver of the Delete button), then spring back.
                        SequentialAnimation {
                            id: peekAnim
                            NumberAnimation { target: menuRow; property: "peekOffset"; to: -menuRow.actionW * 0.5; duration: 200; easing.type: Easing.OutQuad }
                            NumberAnimation { target: menuRow; property: "peekOffset"; to: 0; duration: 250; easing.type: Easing.OutBack }
                        }

                        // Bottom layer: the Delete button (visual only, no input).
                        Item {
                            id: actionsBox
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: menuRow.actionW
                            // Only drawn while the row is slid left, so at rest nothing sits under the
                            // (transparent) row and the list's rounded corners stay clean.
                            visible: rowContent.x < -0.5

                            // Rounded pill inset inside the row, so its corners never poke out of the
                            // drop-down's rounded corners (clip only clips the bounding box).
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingSmall
                                radius: Math.max(0, page.cornerRadius - Theme.paddingSmall)
                                color: "#a8465a"
                                // Visual only: the row's HSwipeArea (above) decides taps by x position.
                                IconButton {
                                    anchors.centerIn: parent
                                    icon.source: "image://theme/icon-m-delete"
                                    enabled: false
                                }
                            }
                        }

                        // Middle layer: row content (name, or inline TextField while renaming).
                        // Opaque, so it covers the buttons until the row is swiped open.
                        Item {
                            id: rowContent
                            width: menuRow.width
                            height: menuRow.height
                            x: (menuRow.dragging ? menuRow.dragX : (menuRow.isOpen ? menuRow.openX : 0)) + menuRow.peekOffset
                            Behavior on x {
                                enabled: !menuRow.dragging
                                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
                            }

                            // Opaque backing only while slid (covers the Delete pill). At rest the row is
                            // transparent, so square row corners never cut into the list's rounded corners.
                            Rectangle {
                                anchors.fill: parent
                                radius: page.cornerRadius
                                color: page.panelColor
                                visible: rowContent.x < -0.5
                            }
                            // Press feedback: accent tint while the finger is down and the press has
                            // not turned into a swipe, a vertical scroll or a long press (rename).
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: Theme.paddingSmall
                                radius: Math.max(0, page.cornerRadius - Theme.paddingSmall)
                                color: page.pressFill
                                opacity: (rowArea.pressed && rowArea.mode === 0 && !rowArea.longFired) ? 1 : 0
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 2 * Theme.paddingLarge
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                text: modelData.name
                                visible: !menuRow.isRenaming
                                font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK
                                color: modelData.index === page.currentSession
                                       ? page.accentColor : page.textColor
                            }

                            TextField {
                                id: renameField
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                visible: menuRow.isRenaming
                                enabled: menuRow.isRenaming
                                // Same look as the row names: centred, app font, primary colour.
                                // No Silica label (it would reserve space above the text).
                                labelVisible: false
                                horizontalAlignment: TextInput.AlignHCenter
                                textLeftMargin: Theme.paddingSmall
                                textRightMargin: Theme.paddingSmall
                                color: page.textColor
                                text: modelData.name
                                font.pixelSize: Theme.fontSizeLarge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeLarge) * page.trackK
                                EnterKey.onClicked: {
                                    page.confirmRename(menuRow.rowId, text);
                                    focus = false;
                                }
                                onVisibleChanged: {
                                    if (visible) {
                                        text = modelData.name;
                                        page.renameFieldItem = renameField;
                                        forceActiveFocus();
                                    }
                                }
                                // Focus lost (tap outside): leave rename mode without saving.
                                // Deferred: changing renamingId synchronously here re-enters the
                                // isRenaming binding (binding loop) and can cancel a rename that
                                // is just starting while focus settles.
                                onActiveFocusChanged: if (!activeFocus) renameBlurTimer.restart()
                                Timer {
                                    id: renameBlurTimer
                                    interval: 50
                                    onTriggered: {
                                        if (menuRow.isRenaming && !renameField.activeFocus) page.cancelRename();
                                    }
                                }
                            }
                        }

                        // Top layer: the swipe / tap / long-press area. It receives every press on the row.
                        //   swipe left (locked horizontal)   -> reveal Delete
                        //   tap on the revealed Delete       -> delete the session
                        //   tap on an open row elsewhere     -> close that row
                        //   tap on a closed row              -> switch to that session, close menu
                        //   long press (600 ms, no drag)     -> inline rename (Enter saves)
                        HSwipeArea {
                            id: rowArea
                            anchors.fill: parent
                            flickable: flick
                            flickable2: menuList
                            enabled: !menuRow.isRenaming
                            holdMs: 600
                            onLongPress: page.startRename(menuRow.rowId)
                            onDragStart: {
                                // Start from where the row is now, so nothing jumps. While dragging the
                                // row follows the finger (its x Behavior is disabled).
                                peekAnim.stop();
                                menuRow.peekOffset = 0;
                                menuRow.baseX = menuRow.isOpen ? menuRow.openX : 0;
                                menuRow.dragX = menuRow.baseX;
                                menuRow.dragging = true;
                            }
                            onDragMove: menuRow.dragX = Math.max(menuRow.openX, Math.min(0, menuRow.baseX + dx))
                            onDragEnd: {
                                var fin = Math.max(menuRow.openX, Math.min(0, menuRow.baseX + dx));
                                // Set the open state BEFORE leaving drag mode: x then animates once,
                                // from the finger position to the final position (no double move).
                                if (fin < menuRow.openX / 2) page.openRowId = menuRow.rowId;
                                else if (page.openRowId === menuRow.rowId) page.openRowId = -1;
                                menuRow.dragging = false;
                            }
                            onClicked: {
                                if (rowArea.swiped || rowArea.longFired) return;   // a swipe / long press is not a tap
                                if (menuRow.isOpen) {
                                    if (mouse.x > menuRow.width - menuRow.actionW) page.deleteSessionById(menuRow.rowId);
                                    else page.openRowId = -1;
                                    return;
                                }
                                if (page.openRowId !== -1) page.openRowId = -1;
                                else page.pickSession(modelData.index);
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- full-page stop layer (only while running) -----------------------
    // Above everything except the popup: while a solve runs, a press ANYWHERE (incl. the
    // hidden scramble area and the stats/history table) stops it at once. The table and other
    // controls underneath never see the touch. The same touch's release stays with this area,
    // so it cannot arm a new hold.
    MouseArea {
        id: runStopLayer
        anchors.fill: parent
        z: 15
        enabled: page.phase === "running"
        visible: enabled
        onPressed: {
            page.stopSolve(Date.now());
            stopRipple.cx = mouse.x;
            stopRipple.cy = mouse.y;
            stopFeedback.restart();
        }
    }

    // ---- stop feedback: a soft, blurred ring spreading from the finger ----------------------
    // Like the hold highlight's soft edge: no hard line, a Gaussian band (ShaderEffect). The
    // effect item is a SQUARE centred on the touch point and the shader works in its own unit
    // square, so the ring is always a perfect circle. It grows to rEnd (not the whole screen)
    // while fading out over the same time. Visual only (never takes input); visible only while
    // animating, so the shader costs nothing when idle.
    ShaderEffect {
        id: stopRipple
        z: 14
        property real rEnd: page.width * 0.45                    // final ring radius (px)
        property real w: Theme.itemSizeMedium                     // soft band half-width (px)
        property real half: rEnd + 3 * w                          // square half-size: band fits
        property real cx: 0
        property real cy: 0
        x: cx - half; y: cy - half
        width: 2 * half; height: 2 * half
        visible: stopFeedback.running
        property real r: 0                                        // current radius (px)
        property real rN: r / (2 * half)                          // shader units (side = 1)
        property real wN: w / (2 * half)
        property color col: Theme.rgba(page.textColor, 0.10)
        fragmentShader: "
            varying highp vec2 qt_TexCoord0;
            uniform lowp float qt_Opacity;
            uniform highp float rN;
            uniform highp float wN;
            uniform lowp vec4 col;
            void main() {
                highp float d = distance(qt_TexCoord0, vec2(0.5, 0.5));
                highp float k = (d - rN) / wN;
                gl_FragColor = col * exp(-k * k) * qt_Opacity;
            }"
    }
    ParallelAnimation {
        id: stopFeedback
        NumberAnimation { target: stopRipple; property: "r"; from: 0; to: stopRipple.rEnd; duration: 380; easing.type: Easing.OutQuad }
        NumberAnimation { target: stopRipple; property: "opacity"; from: 1; to: 0; duration: 380; easing.type: Easing.Linear }
    }

    // ---- rename input blocker (only while renaming a session) -------------
    // Full-page layer above the drop-downs and all page content (below the popup). A press
    // outside the rename field cancels the rename and is swallowed, so nothing underneath
    // (buttons, session rows, '+', event selector, touch area, pull-down) reacts. A press
    // inside the field is not accepted, so it passes through to the TextField.
    // The keyboard is system UI (the page is not resized), so it is not covered by the app.
    MouseArea {
        id: renameBlocker
        anchors.fill: parent
        z: 16
        enabled: page.renamingId !== -1
        visible: enabled
        onPressed: {
            var f = page.renameFieldItem;
            if (f && f.visible) {
                var p = mapToItem(f, mouse.x, mouse.y);
                if (p.x >= 0 && p.y >= 0 && p.x <= f.width && p.y <= f.height) {
                    mouse.accepted = false;
                    return;
                }
            }
            if (f) f.focus = false;
            page.cancelRename();
        }
    }

    // ---- solve detail popup overlay -----------------------------------
    // Full-page layer above everything: dims the page and eats every touch while shown,
    // so nothing underneath (solve start, pull-down, table) sees it. Tap the dim area to close (taps on the card do nothing).
    Item {
        id: solvePopup
        anchors.fill: parent
        z: 20
        opacity: page.popupOpen ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 150 } }

        Rectangle { anchors.fill: parent; color: "#000000"; opacity: 0.6 }
        MouseArea { anchors.fill: parent; onClicked: page.closeSolvePopup() }

        Rectangle {
            id: solveCard
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.horizontalPageMargin
            height: cardCol.height + 2 * Theme.paddingLarge
            color: page.popupCardColor              // opaque (user: the solve popup must not be see-through)
            radius: page.cornerRadius
            scale: page.popupOpen ? 1 : 0.9
            Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
            MouseArea { anchors.fill: parent }      // taps on the card do not close it

            Column {
                id: cardCol
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.paddingLarge
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width - Theme.iconSizeSmall * 1.5
                    text: page.popupSolve ? ("#" + page.popupN + "  \u00b7  " + page.eventName(page.popupSolve.event)) : ""
                    color: page.lavenderColor
                    font.pixelSize: Theme.fontSizeExtraSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeExtraSmall) * page.trackK
                    horizontalAlignment: Text.AlignLeft
                }
                // Time line: big main value, smaller suffix on the same line, centred as a unit.
                Item {
                    width: parent.width
                    height: popupBig.height
                    Label {
                        id: popupBig
                        width: Math.min(implicitWidth, cardCol.width - popupSuffix.width - Theme.paddingSmall)
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.horizontalCenterOffset: -(popupSuffix.width + Theme.paddingSmall) / 2
                        text: page.popupMain(page.popupSolve)
                        color: page.textColor
                        fontSizeMode: Text.HorizontalFit
                        minimumPixelSize: Theme.fontSizeMedium
                        font.pixelSize: Theme.fontSizeHuge; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeHuge) * page.trackBigK
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        id: popupSuffix
                        visible: text !== ""
                        anchors.left: popupBig.right
                        anchors.leftMargin: Theme.paddingSmall
                        anchors.baseline: popupBig.baseline
                        text: page.popupSuffix(page.popupSolve)
                        color: page.lavenderColor
                        font.pixelSize: Theme.fontSizeMedium; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeMedium) * page.trackK
                    }
                }
                Item { width: 1; height: Theme.paddingMedium }
                Item {
                    id: popupScramble
                    width: parent.width
                    height: popupGrid.height
                    ScrambleGrid {
                        id: popupGrid
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: page.scrambleOf(page.popupSolve)
                        eventId: page.popupSolve ? page.popupSolve.event : ""
                        maxWidth: cardCol.width
                        maxHeight: page.height * 0.3
                        fontFamily: page.appFont
                        ink: page.copyFlash ? page.accentColor : page.textColor
                    }
                    // Long press copies; the popup stays open. Presses here never reach the card/dim.
                    MouseArea {
                        anchors.fill: parent
                        property real pressX: 0
                        property real pressY: 0
                        onPressed: {
                            pressX = mouse.x;
                            pressY = mouse.y;
                            popupHoldTimer.restart();
                        }
                        onPositionChanged: {
                            var slop = Theme.startDragDistance || 15;
                            if (Math.abs(mouse.x - pressX) >= slop || Math.abs(mouse.y - pressY) >= slop)
                                popupHoldTimer.stop();
                        }
                        onReleased: popupHoldTimer.stop()
                        onCanceled: popupHoldTimer.stop()
                    }
                }
            }

            // Delete mark, top-right corner: removes THIS solve immediately (closes the popup).
            Item {
                id: popupDelete
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.paddingSmall
                width: Theme.iconSizeSmall * 1.3
                height: width
                // Trash icon tinted via the theme image provider ("?<color>" suffix).
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeSmall * 1.2
                    height: width
                    sourceSize.width: width
                    sourceSize.height: height
                    source: "image://theme/icon-m-delete?" + (deleteMarkArea.pressed ? page.accentColor : page.dimColor)
                }
                MouseArea {
                    id: deleteMarkArea
                    anchors.fill: parent
                    anchors.margins: -Theme.paddingSmall
                    onClicked: page.deleteSolveAt(page.popupIndex)
                }
            }

        }
    }

    // ---- copy feedback: flash, popup hold timer, 'Copied' notice --------
    Timer { id: copyFlashTimer; interval: 300; repeat: false; onTriggered: page.copyFlash = false }
    Timer { id: copyNoticeTimer; interval: 1200; repeat: false; onTriggered: copyNotice.opacity = 0 }
    Timer {
        id: popupHoldTimer
        interval: page.copyHoldMs
        repeat: false
        onTriggered: page.copyScramble(page.scrambleOf(page.popupSolve))
    }
    Label {
        id: copyNotice
        z: 30                                        // above the popup overlay (z 20)
        opacity: 0
        text: qsTr("Copied")
        color: page.accentColor
        font.pixelSize: Theme.fontSizeSmall; font.family: page.appFont; font.letterSpacing: (Theme.fontSizeSmall) * page.trackK
        x: (page.width - width) / 2
        // Under the scramble on the main page, under the card while the popup is open.
        y: page.popupOpen ? (solveCard.y + solveCard.height + Theme.paddingMedium)
                          : (scrambleBox.mapToItem(page, 0, scrambleBox.height).y + Theme.paddingSmall)
        Behavior on opacity { NumberAnimation { duration: 200 } }
    }
}
