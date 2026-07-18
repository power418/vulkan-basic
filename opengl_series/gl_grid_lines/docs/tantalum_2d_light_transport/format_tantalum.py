import re

with open('tantalum.md', 'r', encoding='utf-8') as f:
    text = f.read()

# 1. Clean up weird equation wrappers that my previous script left
text = re.sub(r'\n*\$\$\n*', '$$', text)
text = text.replace('$$', '') # Remove all $$ temporarily so we can re-add them properly
text = re.sub(r'(?<!\$)\$(?!\$)', '', text) # Remove all single $ temporarily

# 2. Add headings
headings2 = [
    'Introduction', 'Related Work', 'The Mathematics of 2D Light Transport',
    'Solutions to the 2D Light Transport Problem', 'Discussion',
    'Reflectance Models in 2D', 'Light Transport on the GPU'
]
headings3 = [
    'Preliminaries', 'Smooth Mirrors and Dielectrics', 'Refraction Coefficients',
    'Lambertian Reflection', 'Phong Shading Model', 'Microfacet Models for Rough Mirrors',
    'Microfacet Models for Rough Dielectrics', 'Wavefront Tracing', 'Overcoming Rasterization Bias'
]

for h in headings2:
    text = re.sub(r'^' + h + r'\s*$', r'\n\n## ' + h + r'\n', text, flags=re.MULTILINE)
for h in headings3:
    text = re.sub(r'^' + h + r'\s*$', r'\n\n### ' + h + r'\n', text, flags=re.MULTILINE)

# 3. Format block equations properly
block_eqs = [
    r'L_i\(\\mathbf{x}, \\omega\) = L_o\(t\(\\mathbf{x}, \\omega\), -\\omega\),',
    r'L_o\(\\mathbf{x}, \\omega_o\) = L_e\(\\mathbf{x}, \\omega_o\) \+ \\int_\{\\mathcal\{H\}\} f_r\(\\omega_i \\rightarrow \\omega_o\) L_i\(\\mathbf{x}, \\omega_i\) \|\\omega_i \\cdot \\mathbf{n}\(\\mathbf{x}\)\| d\\omega_i\.',
    r'\\Phi\(\\mathbf{x}\) = \\int_\{\\Omega\} L_i\(\\mathbf{x}, \\omega\) d\\omega\.',
    r'\\Phi\(\\mathbf{x}\) \\approx \\sum_\{i=1\}\^N \\frac\{L_i\(\\mathbf{x}, \\vec\{X\}_i\)\}\{\\mathrm\{pdf\}\(\\vec\{X\}_i\)\}\.',
    r'\\Phi\(\\mathbf{x}\) \\approx \\sum_\{i=1\}\^N \\frac\{L_o\(t\(\\mathbf{x}, -\\vec\{X\}_i\), \\vec\{X\}_i\)\}\{\\mathrm\{pdf\}\(\\vec\{X\}_i\)\}\.',
    r'I_j = \\int_\{A_j\} h_j\(\\mathbf{x}\) \\Phi\(\\mathbf{x}\) dx\.',
    r'\\int_\{\\mathcal\{H\}\} f_r\(\\omega_i \\leftrightarrow \\omega_o\) \|\\mathbf{n}\(\\mathbf{x}\)\\cdot\\omega_i\| d\\omega_i \\leq 1',
    r'\\omega_i &= \(\\sin\\theta_i,\;\\cos\\theta_i\)  \\omega_o &= \(\\sin\\theta_o,\;\\cos\\theta_o\)\.',
    r'\\int_\{-\\pi/2\}\^\{\\pi/2\} f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i d\\theta_i \\leq 1\.',
    r'\\omega_r = \(-\\sin\\theta_i, \\cos\\theta_i\)\.',
    r'\\omega_t = \(-\\eta \\sin\\theta_i,\,-\\sqrt\{1-\\eta\^2\\sin\^2\\theta_i\}\),',
    r'n\(\\lambda\) = \\sqrt\{1 \+ \\frac\{B_1\\lambda\^2\}\{\\lambda\^2 - C_1\} \+ \\frac\{B_2\\lambda\^2\}\{\\lambda\^2 - C_2\} \+ \\frac\{B_3\\lambda\^2\}\{\\lambda\^2 - C_3\}\}\.',
    r'f_r\(\\omega_i \\leftrightarrow \\omega_o\) = c,',
    r'1 &= \\int_\{-\\pi/2\}\^\{\\pi/2\} f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i d\\theta_i    &= \\int_\{-\\pi/2\}\^\{\\pi/2\} c \\cos\\theta_i d\\theta_i    &= 2c,',
    r'f_r\(\\omega_i \\leftrightarrow \\omega_o\) = \\frac\{1\}\{2\}\.',
    r'\\mathrm\{pdf\}\(\\theta_i\) = f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i = \\frac\{\\cos\\theta_i\}\{2\}\.',
    r'\\mathrm\{cdf\}\(\\theta_i\) &= \\int_\{-\\pi/2\}\^\{\\theta_i\} \\mathrm\{pdf\}\(\\theta\) d\\theta             &= \\int_\{-\\pi/2\}\^\{\\theta_i\} \\frac\{\\cos\\theta\}\{2\} d\\theta             &= \\frac\{1 \+ \\sin\\theta_i\}\{2\}\.',
    r'\\mathrm\{cdf\}\(\\theta_i\) &= \\xi  \\frac\{1 \+ \\sin\\theta_i\}\{2\} &= \\xi  1 \+ \\sin\\theta_i &= 2 \\xi  \\theta_i &= \\arcsin\(2 \\xi - 1\),',
    r'\\mathrm\{cdf\}\^\{-1\}\(\\xi\) = \\arcsin\(2 \\xi - 1\)\.',
    r'f_r\(\\omega_i \\leftrightarrow \\omega_o\) = c\;  \(\\cos\\theta_i\\cos\\theta_o - \\sin\\theta_i\\sin\\theta_o\)\^n,',
    r'\\int_\{-\\pi/2\}\^\{\\pi/2\} f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i d\\theta_i   &= \\int_\{-\\pi/2\}\^\{\\pi/2\} c \\cos\\theta_i\^n \\cos\\theta_i d\\theta_i    &= \\int_\{-\\pi/2\}\^\{\\pi/2\} c \\cos\\theta_i\^\{n \+ 1\} d\\theta_i    &= c \\frac\{\\sqrt\{\\pi\}\;\\Gamma\\left\(\\frac\{2 \+ n\}\{2\}\\right\)\}\{\\Gamma\\left\(\\frac\{3 \+ n\}\{2\}\\right\)\},',
    r'c = \\frac\{\\Gamma\\left\(\\frac\{3 \+ n\}\{2\}\\right\)\}\{\\sqrt\{\\pi\}\;\\Gamma\\left\(\\frac\{2 \+ n\}\{2\}\\right\)\}\.',
    r'\\int_\\Omega f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i d\\omega_i   &= \\int_\{0\}\^\{2 \\pi\} \\int_\{0\}\^\{\\pi/2\} f_r\(\\omega_i \\leftrightarrow \\omega_o\) \\cos\\theta_i \\sin\\theta_i d\\theta_i d\\phi_i    &= \\int_\{0\}\^\{2 \\pi\} \\int_\{0\}\^\{\\pi/2\} c \\cos\\theta_i\^\{n \+ 1\} \\sin\\theta_i d\\theta_i d\\phi_i    &= c \\frac\{2 \\pi\}\{n \+ 2\},',
    r'c = \\frac\{n \+ 2\}\{2 \\pi\},',
    r'f_r\(\\omega_i \\leftrightarrow \\omega_o\) = \\frac\{D\(\\mathbf\{h\}\)G\(\\omega_i, \\omega_o, \\mathbf\{h\}\)\}\{4 \\cos\\theta_i \\cos\\theta_o\},',
    r'D\(\\theta\) = \\frac\{1\}\{4s\}\\coth\\left\(\\frac\{\\pi\}\{4s\}\\right\)\\sech\^2\\left\(\\frac\{\\theta\}\{2s\}\\right),',
    r'\\mathrm\{cdf\}\^\{-1\}\(\\xi\) = 2s\\, \\arctanh\\left\(\(2 \\xi - 1\)\\,\\tanh\\left\(\\frac\{\\pi\}\{4s\}\\right\)\\right\)\.',
    r'\\theta_\{\\min\} &= \\max\(\\theta_o, 0\) - \\frac\{\\pi\}\{2\}  \\theta_\{\\max\} &= \\min\(\\theta_o, 0\)\\, \+ \\frac\{\\pi\}\{2\}\.',
    r'\\mathrm\{cdf\}\^\{-1\}\(\\xi\) = 2s\\, \\arctanh\\left\(\(1 - \\xi\)\\,\\tanh\\left\(\\frac\{\\theta_\{\\min\}\}\{2s\}\\right\) \+ \\xi\\,\\tanh\\left\(\\frac\{\\theta_\{\\max\}\}\{2s\}\\right\)\\right\)\.',
    r'f_r\(\\omega_i \\leftrightarrow \\omega_o\) = \\frac\{F\(\\omega_i, \\mathbf\{h\}\)D\(\\mathbf\{h\}\)G\(\\omega_i, \\omega_o, \\mathbf\{h\}\)\}\{4 \\cos\\theta_i \\cos\\theta_o\},'
]

for eq in block_eqs:
    text = re.sub(r'\s*(' + eq + r')\s*', r'\n\n$$\n\1\n$$\n\n', text)

# Separate sentences that got stuck to equations
text = re.sub(r'(\$\$\n\n)([A-Z])', r'\1\2', text)
text = re.sub(r'([a-z\]\)])(\n\n\$\$)', r'\1\2', text)

# Inline equations like L_i(x, omega), Phi(x), etc.
text = re.sub(r'(?<!\$)(L_[ioe]\([^)]+\)|\Phi\([^)]+\)|t\([^)]+\))(?!\$)', r'$\1$', text)

# Format code blocks
text = text.replace('vec2 sampleMirror(vec2 w_o) {', '\n```glsl\nvec2 sampleMirror(vec2 w_o) {')
text = text.replace('    return vec2(-w_o.x, w_o.y);\n}', '    return vec2(-w_o.x, w_o.y);\n}\n```\n')

text = text.replace('vec2 sampleDielectric(vec2 w_o, float ior) {', '\n```glsl\nvec2 sampleDielectric(vec2 w_o, float ior) {')
text = text.replace('        return vec2(-w_o.x*eta, sqrt(1.0 - eta*eta*w_o.x*w_o.x)*-sign(w_o.y));\n}', '        return vec2(-w_o.x*eta, sqrt(1.0 - eta*eta*w_o.x*w_o.x)*-sign(w_o.y));\n}\n```\n')

text = text.replace('vec2 sampleDiffuse(vec2 w_o) {', '\n```glsl\nvec2 sampleDiffuse(vec2 w_o) {')
text = text.replace('    return vec2(sinThetaI, cosThetaI);\n}', '    return vec2(sinThetaI, cosThetaI);\n}\n```\n')

text = text.replace('float sampleVisibleNormal(float s, float xi, float thetaMin, float thetaMax) {', '\n```glsl\nfloat sampleVisibleNormal(float s, float xi, float thetaMin, float thetaMax) {')
text = text.replace('    return m*(dot(w_o, m)*2.0) - w_o;\n}', '    return m*(dot(w_o, m)*2.0) - w_o;\n}\n```\n')

text = text.replace('vec2 sampleRoughDielectric(vec2 w_o, float ior, float s) {', '\n```glsl\nvec2 sampleRoughDielectric(vec2 w_o, float ior, float s) {')
text = text.replace('        return (etaM*woDotM - cosThetaT)*m - etaM*w_o;\n}', '        return (etaM*woDotM - cosThetaT)*m - etaM*w_o;\n}\n```\n')

text = text.replace('for i = 1 to N:\n    x_i, w_i = sampleLe()', '\n```glsl\nfor i = 1 to N:\n    x_i, w_i = sampleLe()')
text = text.replace('        x_i, w_i = x_i\', w_i\'\nHere, N is', '        x_i, w_i = x_i\', w_i\'\n```\nHere, N is')

text = text.replace('for i = 1 to N/M:\n    lines = []', '\n```glsl\nfor i = 1 to N/M:\n    lines = []')
text = text.replace('            x_i, w_i = x_i\', w_i\'\n    gpuDraw(lines)\nHere, M is', '            x_i, w_i = x_i\', w_i\'\n    gpuDraw(lines)\n```\nHere, M is')

text = text.replace('for i = 1 to N/M:\n    gpuParallelFor(1 to M):\n        x_i, w_i = sampleLe()', '\n```glsl\nfor i = 1 to N/M:\n    gpuParallelFor(1 to M):\n        x_i, w_i = sampleLe()')
text = text.replace('            drawLine(x_i, x_i\')\n            \n            x_i, w_i = x_i\', w_i\'\nI\'ve used gpuParallelFor', '            drawLine(x_i, x_i\')\n            \n            x_i, w_i = x_i\', w_i\'\n```\nI\'ve used gpuParallelFor')

text = text.replace('for i = 1 to N/M:\n    gpuParallelFor(1 to M): x_i, w_i = sampleLe()\n    for j = 1 to p:', '\n```glsl\nfor i = 1 to N/M:\n    gpuParallelFor(1 to M): x_i, w_i = sampleLe()\n    for j = 1 to p:')
text = text.replace('        gpuParallelFor(1 to M): drawLine(x_i, x_i\')\n        x_i, w_i = x_i\', w_i\'\nWe split the "megakernel"', '        gpuParallelFor(1 to M): drawLine(x_i, x_i\')\n        x_i, w_i = x_i\', w_i\'\n```\nWe split the "megakernel"')

text = text.replace('void renderLine(vec2 start, vec2 end) {', '\n```glsl\nvoid renderLine(vec2 start, vec2 end) {')

# Add spacing around paragraphs that have lines crammed
text = re.sub(r'([a-z\.!?>])\n([A-Z])', r'\1\n\n\2', text)

with open('tantalum.md', 'w', encoding='utf-8') as f:
    f.write(text)

print('Formatting done')
