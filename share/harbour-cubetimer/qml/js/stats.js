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
    return aoFromSorted(recent, n);
}

// aoN of a window of n effective times, already sorted ascending.
function aoFromSorted(sorted, n) {
    var drop = Math.ceil(n * 5 / 100);                 // integer maths: 5% of n, rounded up
    var kept = sorted.slice(drop, n - drop);           // drop best AND worst
    if (kept[kept.length - 1] === Infinity) return Infinity;
    var sum = 0;
    for (var k = 0; k < kept.length; k++) sum += kept[k];
    return Math.floor(sum / kept.length / 10) * 10;    // truncate to centiseconds
}

// Index of the first element >= v in the ascending array a.
function lowerBound(a, v) {
    var lo = 0, hi = a.length;
    while (lo < hi) {
        var mid = (lo + hi) >> 1;
        if (a[mid] < v) lo = mid + 1; else hi = mid;
    }
    return lo;
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
    return valueText(aoValueAt(solves, end, n));
}

// Display text of a numeric aoN value (null -> "-", Infinity -> "DNF").
function valueText(v) {
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
// Each aoN keeps its window sorted (binary insert/remove) instead of re-sorting the window for
// every solve, so a session of 1000+ solves costs ~O(n * 100) instead of O(n * 100 log 100).
// Output is identical to aoTextAt (checked in stats_check.js).
function rollingAverages(solves) {
    var WINS = [5, 12, 100];
    var sorted = [[], [], []];
    var vals = [];
    var out = [];
    for (var i = 0; i < solves.length; i++) {
        var v = effectiveMs(solves[i]);
        vals.push(v);
        var texts = [];
        for (var w = 0; w < WINS.length; w++) {
            var n = WINS[w], a = sorted[w];
            a.splice(lowerBound(a, v), 0, v);
            if (i >= n) {
                var old = vals[i - n];                  // leaves the window
                a.splice(lowerBound(a, old), 1);
            }
            texts.push(i + 1 >= n ? valueText(aoFromSorted(a, n)) : "-");
        }
        out.push({ n: i + 1, time: solveText(solves[i]), ao5: texts[0], ao12: texts[1], ao100: texts[2] });
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

// Session bests as display strings: {time, ao5, ao12, ao100}. time = the best single
// (solveText, so a +2 best shows "12.34+"); DNF singles never count. aoN = the lowest aoN over
// every window (DNF averages never count). "-" when there is nothing to show (e.g. all DNF).
function bestStats(solves) {
    var out = { time: "-", ao5: "-", ao12: "-", ao100: "-" };
    if (!solves || solves.length === 0) return out;
    var best = Infinity, bestIdx = -1;
    for (var i = 0; i < solves.length; i++) {
        var v = effectiveMs(solves[i]);
        if (v < best) { best = v; bestIdx = i; }
    }
    if (bestIdx >= 0) out.time = solveText(solves[bestIdx]);
    var WINS = [5, 12, 100], KEYS = ["ao5", "ao12", "ao100"];
    for (var w = 0; w < WINS.length; w++) {
        var n = WINS[w], a = [], vals = [], m = Infinity;
        for (var j = 0; j < solves.length; j++) {
            var x = effectiveMs(solves[j]);
            vals.push(x);
            a.splice(lowerBound(a, x), 0, x);
            if (j >= n) a.splice(lowerBound(a, vals[j - n]), 1);
            if (j + 1 >= n) m = Math.min(m, aoFromSorted(a, n));
        }
        if (m !== Infinity) out[KEYS[w]] = fmtMs(m);
    }
    return out;
}
