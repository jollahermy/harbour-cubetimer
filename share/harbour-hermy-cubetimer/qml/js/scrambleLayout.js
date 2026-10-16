.pragma library

// Scramble layout maths for the cube timer (pure JS, no QML).
//
// Metrics m are measured in QML at 100 px and scale linearly with the pixel size:
//   { tokW, lineH, parenW, numW, commaW, slashW, slashPad }
//   tokW   widest generic token (cell width basis)     lineH  line height
//   parenW '(' / ')' width   numW  widest Square-1 number ('-5' or longer)
//   commaW ',' width         slashW '/' width
//   slashPad  px space on each side of the Square-1 '/' (fixed, not scaled with the font)
//
// Three layouts, all returning { mode, rows, size, width, widest, height, gap, lh, ... }:
//   generic : tokens in fixed-width cells (rows wrap to the width)
//   minx    : megaminx, one WCA line per row, never wrapped
//   sq1     : Square-1 moves "(a,b)" with a '/' column after each move (see layoutSq1)

// Splits a scramble into prepared data for the renderer.
//   mode 'sq1' when eventId is sq1 and the text holds at least one (a,b) move,
//   otherwise 'minx' / 'generic' by event id.
function prepare(text, eventId) {
    var src = String(text);
    if (eventId === "sq1") {
        var moves = parseSq1(src);
        if (moves.length > 0) return { mode: "sq1", moves: moves, lines: [], tokens: [], maxTok: 1, nums: distinctNumbers(moves) };
    }
    var srcLines = src.split("\n");
    var lines = [];
    var tokens = [];
    var maxTok = 1;
    for (var i = 0; i < srcLines.length; i++) {
        var toks = srcLines[i].split(/\s+/).filter(function (t) { return t.length > 0; });
        lines.push(toks);
        maxTok = Math.max(maxTok, toks.length);
        for (var j = 0; j < toks.length; j++) tokens.push(toks[j]);
    }
    return { mode: eventId === "minx" ? "minx" : "generic", moves: [], lines: lines, tokens: tokens, maxTok: maxTok, nums: [] };
}

// Square-1 moves: each "(a,b)" is one move; a '/' belongs to the move before it.
// Returns [{a, b, slash}] with a, b as strings ('-5', '0', '6'). A trailing '/' sets slash on the last move.
function parseSq1(text) {
    var re = /\(\s*(-?\d+)\s*,\s*(-?\d+)\s*\)|\//g;
    var moves = [];
    var m;
    while ((m = re.exec(String(text))) !== null) {
        if (m[0] === "/") {
            if (moves.length > 0) moves[moves.length - 1].slash = true;
        } else {
            moves.push({ a: m[1], b: m[2], slash: false });
        }
    }
    return moves;
}

// Distinct a/b strings of the moves (QML probes them to size the number cells).
function distinctNumbers(moves) {
    var seen = {};
    var out = [];
    for (var i = 0; i < moves.length; i++) {
        var v = [moves[i].a, moves[i].b];
        for (var k = 0; k < 2; k++) {
            if (!seen[v[k]]) { seen[v[k]] = true; out.push(v[k]); }
        }
    }
    return out;
}

// Generic / megaminx layout at a pixel size. Generic events wrap rows to the width; megaminx
// (noWrap) keeps each WCA line in one row. Returns the rows plus the block size.
function layoutAt(prep, m, px, noWrap, width) {
    var cw = m.tokW * px / 100 * 1.04;                // 4 % headroom for hinted advances
    var gap = px * 0.5;
    var lh = m.lineH * px / 100;
    var cols = noWrap ? Math.max(1, prep.maxTok)
                      : Math.max(1, Math.floor((width + gap) / (cw + gap)));
    var rows = [];
    var widest = 0;
    for (var r = 0; r < prep.lines.length; r++) {
        var row = prep.lines[r];
        for (var c = 0; c < row.length; c += cols) {
            var chunk = row.slice(c, c + cols);
            rows.push(chunk);
            widest = Math.max(widest, chunk.length * cw + (chunk.length - 1) * gap);
        }
    }
    return { mode: prep.mode === "minx" ? "minx" : "generic", rows: rows, size: px, cw: cw, gap: gap, lh: lh,
             width: Math.min(widest, width), widest: widest, height: rows.length * lh };
}

// Square-1 layout at a pixel size. A cell is one move "(a,b)" followed by a slash column
// (slash centred, slashPad on each side). The column carries the space between moves, so a '/'
// has the same space to its left and right and the row gap is 0. The numbers sit centred in
// sub-cells sized for the widest number, so the commas line up in every row.
// Rows hold as many cells as fit the width (cells keep their own width, rows are left-aligned).
function layoutSq1(prep, m, px, width) {
    var k = px / 100;
    var parenW = m.parenW * k;
    var numW = m.numW * k;
    var commaW = m.commaW * k;
    var moveW = 2 * parenW + 2 * numW + commaW;
    var padW = m.slashPad || 0;
    var sepW = m.slashW * k + 2 * padW;               // slash column: slash + equal pad each side
    var cellW = moveW + sepW;
    var gap = 0;
    var lh = m.lineH * k;
    var cols = Math.max(1, Math.floor((width + gap) / (cellW + gap)));
    var rows = [];
    for (var c = 0; c < prep.moves.length; c += cols) rows.push(prep.moves.slice(c, c + cols));
    var n = Math.min(cols, prep.moves.length);
    var widest = n * cellW + (n - 1) * gap;
    return { mode: "sq1", rows: rows, size: px, cellW: cellW, moveW: moveW, sepW: sepW,
             parenW: parenW, numW: numW, commaW: commaW, gap: gap, lh: lh,
             width: Math.min(widest, width), widest: widest, height: rows.length * lh };
}

// Megaminx: 7 WCA lines, one row each, never wrapped; largest integer px that fits.
// Null when the metrics are not measured yet.
function fitMegaminx(prep, m, width, maxHeight) {
    if (m.tokW <= 0 || m.lineH <= 0) return null;
    var w100 = layoutAt(prep, m, 100, true, width).widest;
    var h100 = layoutAt(prep, m, 100, true, width).height;
    var pxW = Math.floor(width * 100 / Math.max(1, w100));
    var pxH = Math.floor(maxHeight * 100 / Math.max(1, h100));
    var px = Math.min(pxW, pxH);
    if (px < 8) return null;
    return layoutAt(prep, m, px, true, width);
}

// Entry point: the layout to render for prep at the given box.
//   sizes: pixel sizes, largest first (the QML passes Theme.fontSizeLarge .. ExtraSmall).
function fit(prep, m, width, maxHeight, sizes) {
    if (prep.mode === "sq1") {
        return fitSizesWith(sizes, function (px) { return layoutSq1(prep, m, px, width); }, width, maxHeight);
    }
    if (prep.mode === "minx") {
        var mx = fitMegaminx(prep, m, width, maxHeight);
        if (mx) return mx;
    }
    return fitSizesWith(sizes, function (px) { return layoutAt(prep, m, px, false, width); }, width, maxHeight);
}

function fitSizesWith(sizes, layoutFn, width, maxHeight) {
    var best = null;
    for (var k = 0; k < sizes.length; k++) {
        best = layoutFn(sizes[k]);
        if (best.height <= maxHeight && best.widest <= width + 0.5) break;
    }
    return best;
}

// Plain WCA text for the clipboard: tokens joined by single spaces, rows joined by '\n'.
function plainText(text) {
    var rows = String(text).split("\n");
    var out = [];
    for (var i = 0; i < rows.length; i++) {
        var toks = rows[i].split(/\s+/).filter(function (t) { return t.length > 0; });
        if (toks.length) out.push(toks.join(" "));
    }
    return out.join("\n");
}
