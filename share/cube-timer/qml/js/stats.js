.pragma library

// WCA-style average-of-N (aoN) for the cube timer.
//
// Input: an array of solves, oldest first. Solve: {time_ms, penalty: {plus2Count, dnf}}.
//   effective time = time_ms + 2000 * plus2Count; DNF = Infinity (worst).
// aoN uses the most recent N solves. The best and worst ceil(5% of N) are dropped
// (ao5, ao12: 1 each side; ao100: 5 each side). If more DNFs than dropped-worst
// results remain in the kept set, the average is DNF.
// Averages are truncated to centiseconds, like the single-solve display.

function effectiveMs(s) {
    var pen = s.penalty || {};
    if (pen.dnf) return Infinity;
    return s.time_ms + 2000 * (pen.plus2Count || 0);
}

// Numeric aoN of the window ending at index `end` (inclusive), in ms. Infinity for DNF,
// null when there are fewer than n solves up to and including `end`.
function aoValueAt(solves, end, n) {
    if (!solves || end + 1 < n) return null;
    var recent = [];
    for (var i = end - n + 1; i <= end; i++) recent.push(effectiveMs(solves[i]));
    recent.sort(function (a, b) { return a - b; });   // Infinity sorts last
    var drop = Math.ceil(n * 5 / 100);                 // integer maths: 5% of n, rounded up
    var kept = recent.slice(drop, n - drop);           // drop best AND worst
    if (kept[kept.length - 1] === Infinity) return Infinity;
    var sum = 0;
    for (var k = 0; k < kept.length; k++) sum += kept[k];
    return Math.floor(sum / kept.length / 10) * 10;    // truncate to centiseconds
}

// Numeric aoN of the whole list (latest window), in ms; null if fewer than n solves.
function aoValue(solves, n) {
    return aoValueAt(solves, solves ? solves.length - 1 : -1, n);
}

function pad2(n) { return n < 10 ? "0" + n : "" + n; }

// ms -> "s.cc" (<60 s), "m:ss.cc" (<10 min), "mm:ss.cc" (<1 h), "h:mm:ss.cc" (>=1 h).
// Truncates to centiseconds (WCA style). The one formatter for main display, stats, history, popup.
function fmtMs(ms) {
    var cs = Math.floor(ms / 10);
    var sec = Math.floor(cs / 100);
    var c = cs % 100;
    if (sec < 60) return sec + "." + pad2(c);
    var min = Math.floor(sec / 60);
    if (min < 60) return min + ":" + pad2(sec % 60) + "." + pad2(c);   // 10..59 min already 2 digits
    return Math.floor(min / 60) + ":" + pad2(min % 60) + ":" + pad2(sec % 60) + "." + pad2(c);
}

// Display text for aoN ending at index `end`: "12.34", "m:ss.cc", "DNF", or "-".
function aoTextAt(solves, end, n) {
    var v = aoValueAt(solves, end, n);
    if (v === null) return "-";
    if (v === Infinity) return "DNF";
    return fmtMs(v);
}

// Display text for aoN: "12.34", "m:ss.cc", "DNF", or "-" if fewer than n solves.
function aoText(solves, n) {
    return aoTextAt(solves, solves ? solves.length - 1 : -1, n);
}

// Display text of one solve: "12.34", "1:02.34", "12.34+" with +2 (the penalised total,
// like the main display), "DNF", or "-" when there is no solve.
function solveText(s) {
    if (!s) return "-";
    var pen = s.penalty || {};
    if (pen.dnf) return "DNF";
    if (pen.plus2Count > 0) return fmtMs(s.time_ms + 2000 * pen.plus2Count) + "+";
    return fmtMs(s.time_ms);
}

// Per solve (oldest first): {n: 1-based index, time, ao5, ao12, ao100} as display strings.
// Each aoN is the average ending at that solve ("-" where fewer solves). O(n * 100 log 100).
function rollingAverages(solves) {
    var out = [];
    for (var i = 0; i < solves.length; i++) {
        out.push({ n: i + 1, time: solveText(solves[i]),
                   ao5: aoTextAt(solves, i, 5), ao12: aoTextAt(solves, i, 12), ao100: aoTextAt(solves, i, 100) });
    }
    return out;
}

// A string of '8's as wide as the longest display text in rows (used to size table columns).
function widthProbeText(rows) {
    var m = 1;
    for (var i = 0; i < rows.length; i++) {
        var r = rows[i];
        m = Math.max(m, r.time.length, r.ao5.length, r.ao12.length, r.ao100.length);
    }
    var s = "";
    for (var k = 0; k < m; k++) s += "8";
    return s;
}

// Three display strings for the stats block.
function computeStats(solves) {
    return { ao5: aoText(solves, 5), ao12: aoText(solves, 12), ao100: aoText(solves, 100) };
}
