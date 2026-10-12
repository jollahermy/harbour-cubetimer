.pragma library

// WCA-style scramble generator for the cube timer.
//
// IMPORTANT: these are RANDOM-MOVE scrambles, not random-state scrambles.
// Each scramble is a random sequence of moves with the WCA suffixes
// ('', ''' and '2'), it is NOT derived from a uniformly random cube state.
// Only a few simple sequence rules are enforced:
//   - never the same face twice in a row (wide "Rw" / "3Rw" count as face R)
//   - never three consecutive moves on the same axis (e.g. R L R)
// Square-1 is only "valid-looking": it does NOT check slice legality.
// Clock uses the fixed WCA move string plus a random pin list.

function rndInt(lo, hi) {               // inclusive
    return lo + Math.floor(Math.random() * (hi - lo + 1));
}

function pick(list) {
    return list[Math.floor(Math.random() * list.length)];
}

// Base face of a token: "Rw" -> "R", "3Rw" -> "R", "R" -> "R".
function baseFace(tok) {
    return tok.replace(/^[0-9]+/, "").replace(/w$/, "");
}

// Rotation axis of a cube face. Unknown faces map to themselves.
function axisOf(face) {
    if (face === "R" || face === "L") return "x";
    if (face === "U" || face === "D") return "y";
    if (face === "F" || face === "B") return "z";
    return face;
}

// Random-move generator: `count` moves drawn from `tokens`, each with a
// random suffix from `suffixes`, obeying the no-repeat / no-3-same-axis rules.
function randomMoves(count, tokens, suffixes, useAxis) {
    var moves = [];
    var faces = [];
    var guard = 0;
    while (moves.length < count) {
        guard++;
        if (guard > 100000) break;          // safety net, never hit in practice
        var tok = pick(tokens);
        var f = baseFace(tok);
        var n = faces.length;
        if (n >= 1 && faces[n - 1] === f) continue;
        if (useAxis && n >= 2 &&
            axisOf(faces[n - 1]) === axisOf(f) &&
            axisOf(faces[n - 2]) === axisOf(f)) continue;
        moves.push(tok + pick(suffixes));
        faces.push(f);
    }
    return moves.join(" ");
}

var CUBE_SUFFIX = ["", "'", "2"];
var SHORT_SUFFIX = ["", "'"];

function scrambleNxN(count, tokens) {
    return randomMoves(count, tokens, CUBE_SUFFIX, true);
}

function scramble2x2() {
    return scrambleNxN(rndInt(9, 11), ["R", "U", "F"]);
}

function scramble3x3() {
    return scrambleNxN(20, ["R", "L", "U", "D", "F", "B"]);
}

function scramble4x4() {
    return scrambleNxN(40, ["R", "L", "U", "D", "F", "B",
                            "Rw", "Uw", "Fw"]);
}

function scramble5x5() {
    return scrambleNxN(60, ["R", "L", "U", "D", "F", "B",
                            "Rw", "Lw", "Uw", "Dw", "Fw", "Bw"]);
}

function scramble6x6() {
    // Outer faces, 2-layer wide and 3-layer wide ("3Rw" style) moves.
    return scrambleNxN(80, ["R", "L", "U", "D", "F", "B",
                            "Rw", "Lw", "Uw", "Dw", "Fw", "Bw",
                            "3Rw", "3Lw", "3Uw", "3Dw", "3Fw", "3Bw"]);
}

function scramble7x7() {
    return scrambleNxN(100, ["R", "L", "U", "D", "F", "B",
                             "2Rw", "2Lw", "2Uw", "2Dw", "2Fw", "2Bw",
                             "3Rw", "3Lw", "3Uw", "3Dw", "3Fw", "3Bw"]);
}

function scramblePyraminx() {
    // 8-10 tip-less moves (U L R B, ' variants, no same face twice in a row),
    // then each tip is turned 0, 1 or 2 times with a random direction.
    var s = randomMoves(rndInt(8, 10), ["U", "L", "R", "B"], SHORT_SUFFIX, false);
    var tips = ["u", "l", "r", "b"];
    var out = [];
    for (var i = 0; i < tips.length; i++) {
        var times = rndInt(0, 2);
        for (var k = 0; k < times; k++) {
            out.push(tips[i] + pick(SHORT_SUFFIX));
        }
    }
    return s + (out.length ? " " + out.join(" ") : "");
}

function scrambleSkewb() {
    return randomMoves(rndInt(9, 11), ["R", "U", "L", "B"], SHORT_SUFFIX, false);
}

function scrambleMegaminx() {
    // WCA format: 7 lines. Each line: 10 alternating R/D moves with random
    // ++ or -- suffixes, then U or U' at the end of the line.
    var lines = [];
    for (var l = 0; l < 7; l++) {
        var parts = [];
        for (var i = 0; i < 10; i++) {
            var face = (i % 2 === 0) ? "R" : "D";
            parts.push(face + (Math.random() < 0.5 ? "++" : "--"));
        }
        parts.push(Math.random() < 0.5 ? "U" : "U'");
        lines.push(parts.join(" "));
    }
    return lines.join("\n");
}

function scrambleSquare1() {
    // About 12 (x,y) tuples, each coordinate in -5..6 (U-layer slice units).
    // Tuples never equal (0,0). No slice-legality check is performed.
    var n = rndInt(11, 13);
    var tuples = [];
    while (tuples.length < n) {
        var x = rndInt(-5, 6);
        var y = rndInt(-5, 6);
        if (x === 0 && y === 0) continue;
        tuples.push("(" + x + "," + y + ")");
    }
    return tuples.join(" / ");
}

function scrambleClock() {
    var fixed = "UR2+ DR3- DL1+ UL0+ U4- R2+ D5- L0+ ALL3+ y2 " +
                "U1+ R0- D3+ L2- ALL5-";
    var corners = ["UR", "DR", "DL", "UL"];
    var pins = [];
    for (var i = 0; i < corners.length; i++) {
        if (Math.random() < 0.5) pins.push(corners[i]);
    }
    if (pins.length < 2) pins = ["UR", "DL"];   // keep it simple, never empty
    return fixed + " " + pins.join(" ");
}

// Public entry point. eventId matches the ids used in the QML event list.
// Unknown ids fall back to a 3x3 scramble.
function generate(eventId) {
    switch (eventId) {
    case "222":    return scramble2x2();
    case "333":    return scramble3x3();
    case "333oh":  return scramble3x3();   // OH uses the 3x3 scramble
    case "333bld": return scramble3x3();   // BLD uses the 3x3 scramble
    case "444":    return scramble4x4();
    case "555":    return scramble5x5();
    case "666":    return scramble6x6();
    case "777":    return scramble7x7();
    case "pyram":  return scramblePyraminx();
    case "skewb":  return scrambleSkewb();
    case "minx":   return scrambleMegaminx();
    case "sq1":    return scrambleSquare1();
    case "clock":  return scrambleClock();
    default:       return scramble3x3();
    }
}
