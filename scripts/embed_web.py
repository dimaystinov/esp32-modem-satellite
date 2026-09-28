from pathlib import Path
root = Path(__file__).resolve().parents[1] / 'firmware/modem_c3'
parts = ['#pragma once\nnamespace WebAssets {\n']
for name, symbol in [('index.html', 'INDEX_HTML'), ('style.css', 'STYLE_CSS'), ('app.js', 'APP_JS')]:
    text = (root / 'data' / name).read_text()
    assert ')WEB"' not in text
    parts.append(f'static const char {symbol}[] = R"WEB({text})WEB";\n')
parts.append('}\n')
(root / 'src/WebAssets.h').write_text(''.join(parts))
