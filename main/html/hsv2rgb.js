// ==ClosureCompiler==
// @compilation_level ADVANCED_OPTIMIZATIONS
// @output_file_name default.js
// ==/ClosureCompiler==
function hsv2rgb( c ) {
    var m = Math, u = m.max, w = m.min, x = m.round, z = m.PI;
    function set(r, g, b, out) {
        out['r'] = x(r * 255);
        out['g'] = x(g * 255);
        out['b'] = x(b * 255);
    }

    out = { 'r':0, 'g': 0, 'b': 0 };
    var h = (c['h'] / z * 3) % 6;
    var s = u(0, w(c['s'], 1));
    var v = u(0, w(c['v'], 1));

    // Grey
    if( !s ) {
        out['r'] = out['g'] = out['b'] = Math.ceil(v * 255);
    } else {
        var b = ((1 - s) * v);
        var vb = v - b;
        var hm = h % 1;
        var y = h|0;
        if(y == 0) set(v, vb * h + b, b, out);
        if(y == 1) set(vb * (1 - hm) + b, v, b, out);
        if(y == 2) set(b, v, vb * hm + b, out);
        if(y == 3) set(b, vb * (1 - hm) + b, v, out);
        if(y == 4) set(vb * hm + b, b, v, out);
        if(y == 5) set(v, b, vb * (1 - hm) + b, out);
    }
    return out;
}
window['hsv2rgb'] = hsv2rgb;
