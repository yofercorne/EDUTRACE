/* ================================================================
   EduTrace App — app.js
   Matches exact output formats of the EduTrace compiler CLI.

   Token format:   TokenTypeName 'lexeme' (line:col)
   AST format:     indented text via ASTPrinter (spaces * level)
   Check format:   "Análisis léxico, sintáctico y semántico exitoso."
   Trace format:   lines starting with "=== EduTrace..." header
   Opt format:     "Resumen de optimizaciones\n  Constant folding: N\n..."
   ASM:            data.asm field (written to file, read by server.py)
   Run:            data.stdout = program stdout, data.asm = generated asm
================================================================ */

'use strict';

// ── ELEMENTS ─────────────────────────────────────────────────
const $  = id => document.getElementById(id);
const codeEditor = $('codeEditor');
const lineNums   = $('lineNums');
const cursorPos  = $('cursorPos');
const charCount  = $('charCount');
const sbDot      = $('sbDot');
const sbMsg      = $('sbMsg');
const sbElapsed  = $('sbElapsed');
const errBadge   = $('errBadge');
const healthDot  = $('healthDot');
const mainRunBtn = $('mainRunBtn');

// ── SAMPLE ───────────────────────────────────────────────────
const SAMPLE = `fun int main() {
    let x = 10;
    let y = x + 5;
    var int nums[3];

    nums[0] = x;
    nums[1] = y;
    nums[2] = nums[0] + nums[1];

    trace variables(x, y);
    trace array(nums);

    if (nums[2] > 20) {
        print("mayor a 20");
    } else {
        print("menor o igual a 20");
    }

    print(nums[2]);
    return 0;
}`;

// ── TAB / PANEL SYSTEM ───────────────────────────────────────
function showTab(tabId) {
  document.querySelectorAll('.tab').forEach(t =>
    t.classList.toggle('active', t.dataset.tab === tabId));
  document.querySelectorAll('.panel').forEach(p =>
    p.classList.toggle('active', p.id === `panel-${tabId}`));
}

document.querySelectorAll('.tab').forEach(btn =>
  btn.addEventListener('click', () => showTab(btn.dataset.tab)));

// ── PIPELINE STATE ────────────────────────────────────────────
// action -> pipe-step button via data-action
function setPipe(action, state) {
  // state: '' | 'active' | 's-ok' | 's-err' | 's-spin'
  const btn = document.querySelector(`.ps[data-action="${action}"]`);
  if (!btn) return;
  btn.classList.remove('active', 's-ok', 's-err', 's-spin');
  if (state) btn.classList.add(state);
}

// ── STATUS BAR ────────────────────────────────────────────────
function setStatus(msg, state = '') {
  sbMsg.textContent = msg;
  sbMsg.className = `sb-msg ${state}`;
  sbDot.className  = `sb-dot ${state === 'ok' ? 'ok' : state === 'err' ? 'err' : state === 'loading' ? 'spin' : ''}`;
}

function setElapsed(ms) {
  if (ms == null) { sbElapsed.textContent = ''; return; }
  sbElapsed.textContent = ms < 1000 ? `${ms}ms` : `${(ms/1000).toFixed(2)}s`;
}

// ── RESULT HELPERS ────────────────────────────────────────────
function h(s) {
  return String(s)
    .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function showEmpty(panelId) {
  $(`es-${panelId}`)  && ($(`es-${panelId}`).style.display = '');
  $(`res-${panelId}`) && $(`res-${panelId}`).classList.add('hidden');
}

function showResult(panelId) {
  $(`es-${panelId}`)  && ($(`es-${panelId}`).style.display = 'none');
  $(`res-${panelId}`) && $(`res-${panelId}`).classList.remove('hidden');
}

function makeHdr(ok, label, ms) {
  const tag = ok
    ? `<span class="tag tag-ok">✓ OK</span>`
    : `<span class="tag tag-err">✗ Error</span>`;
  const time = ms != null ? `<span class="ms-label">${ms < 1000 ? ms+'ms' : (ms/1000).toFixed(2)+'s'}</span>` : '';
  return `${tag}<span>${h(label)}</span>${time}`;
}

// ── ERRORS PANEL ──────────────────────────────────────────────
let errTotal = 0;

function addError(source, msg) {
  if (!msg || !msg.trim()) return;
  errTotal++;
  errBadge.hidden = false;
  errBadge.textContent = errTotal;
  showResult('err');
  const list = $('err-list');
  const item = document.createElement('div');
  item.className = 'err-item';
  item.innerHTML = `<div class="err-src">${h(source)}</div><div class="err-msg">${h(msg.trim())}</div>`;
  list.appendChild(item);
}

function clearAllResults() {
  // reset all panels to empty state
  ['check','tokens','ast','trace','opt','asm','noopt','run','err'].forEach(id => showEmpty(id));
  // clear content
  ['pre-check','pre-tokens','pre-ast','pre-asm','pre-noopt','pre-run'].forEach(id => {
    const el = $(id); if (el) el.innerHTML = '';
  });
  ['tok-chips','trace-view','opt-view','err-list'].forEach(id => {
    const el = $(id); if (el) el.innerHTML = '';
  });
  ['hdr-check','hdr-tokens','hdr-ast','hdr-trace','hdr-opt','hdr-asm','hdr-noopt','hdr-run'].forEach(id => {
    const el = $(id); if (el) el.innerHTML = '';
  });
  errTotal = 0;
  errBadge.hidden = true;
  errBadge.textContent = '0';
  document.querySelectorAll('.ps').forEach(b => setPipe(b.dataset.action, ''));
  setStatus('Listo');
  setElapsed(null);
}

// ── TOKEN PARSER ──────────────────────────────────────────────
/*
  Real format from scanner.cpp / main.cpp:
    tokenTypeName(t.type) << " '" << t.lexeme << "' (" << t.loc.toString() << ")"
  Example:
    Fun 'fun' (1:1)
    Int 'int' (1:5)
    Identifier 'main' (1:9)
    LeftParen '(' (1:13)
*/

// Map tokenTypeName -> CSS class
const KW_NAMES = new Set([
  'Fun','Var','Let','If','Else','While','For','Return','Print',
  'New','Delete','Lambda','True','False'
]);
const TYPE_NAMES = new Set(['Int','Bool','Char','StringType','Void']);
const PUNCT_NAMES = new Set([
  'LeftParen','RightParen','LeftBrace','RightBrace',
  'LeftBracket','RightBracket','Comma','Dot','Semicolon','Colon'
]);
const OP_NAMES = new Set([
  'Plus','Minus','Star','Slash','Percent','Ampersand',
  'Bang','BangEqual','Equal','EqualEqual',
  'Greater','GreaterEqual','Less','LessEqual','AndAnd','OrOr'
]);
const EDU_NAMES = new Set([
  'Trace','Variables','Loop','Branch','Stack','Recursion','Array','Memory'
]);

function tokenClass(typeName) {
  if (typeName === 'Identifier')    return 'tk-id';
  if (typeName === 'Number')        return 'tk-num';
  if (typeName === 'StringLiteral') return 'tk-str';
  if (typeName === 'CharLiteral')   return 'tk-str';
  if (typeName === 'EndOfFile')     return 'tk-eof';
  if (KW_NAMES.has(typeName))       return 'tk-kw';
  if (TYPE_NAMES.has(typeName))     return 'tk-type';
  if (PUNCT_NAMES.has(typeName))    return 'tk-pun';
  if (OP_NAMES.has(typeName))       return 'tk-op';
  if (EDU_NAMES.has(typeName))      return 'tk-edu';
  return 'tk-pun';
}

function renderTokens(raw) {
  const chips = $('tok-chips');
  const pre   = $('pre-tokens');
  chips.innerHTML = '';

  // Try to parse "TypeName 'lexeme' (line:col)" format
  // Regex: captures: (typename) '(lexeme)' (loc)
  const RE = /^([A-Za-z]+)\s+'(.*)'\s+\((.+)\)$/;
  const lines = raw.split('\n').filter(l => l.trim());
  let parsed = 0;

  lines.forEach(line => {
    const m = line.match(RE);
    if (!m) return;
    parsed++;
    const [, tname, lexeme, loc] = m;
    const cls = tokenClass(tname);
    const chip = document.createElement('span');
    chip.className = `tk ${cls}`;
    chip.title = `${tname} @ ${loc}`;

    // Show: lexeme for readable tokens, or type name for punctuation/operators
    const displayLex = lexeme.length <= 20 ? lexeme : lexeme.slice(0,18)+'…';
    const showType = cls === 'tk-pun' || cls === 'tk-op' || cls === 'tk-eof';

    if (showType) {
      chip.innerHTML = `<span>${h(tname)}</span>${lexeme && lexeme !== tname ? `<span class="tk-lex">${h(displayLex)}</span>` : ''}`;
    } else {
      chip.innerHTML = `<span>${h(displayLex)}</span><span class="tk-lex">${h(tname)}</span>`;
    }
    chips.appendChild(chip);
  });

  if (parsed === 0) {
    // fallback: raw text
    chips.classList.add('hidden');
    pre.classList.remove('hidden');
    pre.textContent = raw;
  } else {
    chips.classList.remove('hidden');
    pre.classList.add('hidden');
  }
}

// ── AST COLORIZER ─────────────────────────────────────────────
/*
  Real format from ASTPrinter in main.cpp, uses 2-space indentation per level.
  Lines like:
    Program
      Function main -> int
        Param x: int
        Block
          LetDecl x: int
            Int 10
          ...
*/
function colorizeAST(raw) {
  return raw.split('\n').map(line => {
    const indent = line.match(/^(\s*)/)[1];
    const rest   = line.slice(indent.length);
    if (!rest) return h(line);

    // tree connectors (if any, e.g. from └── style)
    const treeM = rest.match(/^([└├│─\s]+)(.*)/);
    const tree  = treeM ? `<span class="ak-tree">${h(treeM[1])}</span>` : '';
    const content = treeM ? treeM[2] : rest;

    let colored = '';

    if (/^Program/.test(content))
      colored = `<span class="ak-blk">${h(content)}</span>`;
    else if (/^(Function|Struct|Param)/.test(content))
      colored = `<span class="ak-fn">${h(content)}</span>`;
    else if (/^(LetDecl|VarDecl|Field)/.test(content))
      colored = `<span class="ak-decl">${h(content)}</span>`;
    else if (/^(Block)/.test(content))
      colored = `<span class="ak-blk">${h(content)}</span>`;
    else if (/^(If|While|For|Return|Print|Assign|ArrayAssign|PointerAssign|FieldAssign|Trace|ExprStmt|Delete)/.test(content))
      colored = `<span class="ak-stmt">${h(content)}</span>`;
    else if (/^(Binary|Unary|Call|ArrayAccess|FieldAccess|Lambda|New|Int|Bool|Char)/.test(content))
      colored = `<span class="ak-expr">${h(content)}</span>`;
    else if (/^Id\s/.test(content))
      colored = `<span class="ak-id">${h(content)}</span>`;
    else if (/^String\s/.test(content))
      colored = `<span class="ak-str">${h(content)}</span>`;
    else
      colored = h(content);

    // highlight operator inside Binary '...'
    colored = colored.replace(/'([^']+)'/g, `'<span class="ak-op">${'$1'}</span>'`);

    return h(indent) + tree + colored;
  }).join('\n');
}

// ── TRACE RENDERER ─────────────────────────────────────────────
/*
  Real lines from trace_visitor.cpp:
  "=== EduTrace: traza educativa ==="
  "=== Fin de traza ==="
  "  Entrando a función: main"
  "  Retorna 0 desde main"
  "  print: 25"
  "  Variable x cambió: 10 -> 15"
  "  Arreglo nums[0] cambió: 0 -> 10"
  "  Evaluando condición if:"
  "    nums[2] > 20 => false"
  "  Se ejecuta la rama ELSE."
  "  Iteración 1 del while"
  "  trace variables:"
  "    x = 10"
  "  trace array activado..."
  "  Inferencia de tipo: x = int"
  "  Declaración: x = 10"
*/
function renderTrace(raw) {
  const view = $('trace-view');
  view.innerHTML = '';

  const lines = raw.split('\n');

  lines.forEach(line => {
    if (!line.trim()) return;

    const trim = line.trimStart();
    let cls = 'te-info', icon = '·', body = '';

    if (/^===/.test(trim)) {
      cls = 'te-sep';
      icon = '━';
      body = `<b>${h(trim.replace(/===/g,'').trim())}</b>`;
    } else if (/^Entrando a (función|lambda):/.test(trim)) {
      cls = 'te-fn'; icon = '→';
      body = `<b>${h(trim)}</b>`;
    } else if (/^Retorna .+ desde /.test(trim)) {
      cls = 'te-ret'; icon = '⏎';
      // Retorna <val> desde <fn>
      const m = trim.match(/^Retorna (.+) desde (.+)/);
      if (m) body = `Retorna <span class="val">${h(m[1])}</span> desde <b>${h(m[2])}</b>`;
      else body = h(trim);
    } else if (/^print:/.test(trim)) {
      cls = 'te-print'; icon = '»';
      const val = trim.slice('print:'.length).trim();
      body = `<b>print</b> <span class="arr">→</span> <span class="val">${h(val)}</span>`;
    } else if (/^Variable .+ cambió:/.test(trim)) {
      cls = 'te-var'; icon = '⟳';
      const m = trim.match(/^Variable (.+?) cambió: (.+?) -> (.+)/);
      if (m) body = `<b>${h(m[1])}</b> <span class="arr">cambió</span> <span class="val">${h(m[2])}</span> <span class="arr">→</span> <span class="val">${h(m[3])}</span>`;
      else body = h(trim);
    } else if (/^Arreglo .+ cambió:/.test(trim)) {
      cls = 'te-arr'; icon = '⟳';
      const m = trim.match(/^Arreglo (.+?) cambió: (.+?) -> (.+)/);
      if (m) body = `<b>${h(m[1])}</b> <span class="arr">cambió</span> <span class="val">${h(m[2])}</span> <span class="arr">→</span> <span class="val">${h(m[3])}</span>`;
      else body = h(trim);
    } else if (/^Evaluando condición (if|while):/.test(trim)) {
      cls = 'te-if'; icon = '?';
      body = `<span class="cond">${h(trim)}</span>`;
    } else if (/^Se ejecuta la rama/.test(trim)) {
      cls = 'te-if'; icon = '↳';
      const branch = trim.includes('THEN') ? 'THEN' : 'ELSE';
      body = `Rama <b class="${branch==='THEN'?'ok':'arr'}">${branch}</b>`;
    } else if (/^Iteración \d+/.test(trim)) {
      cls = 'te-loop'; icon = '↻';
      body = `<b>${h(trim)}</b>`;
    } else if (/^(trace (variables?|array|loop|branch|stack|recursion|memory)|Fin del ciclo|Declaración:|Inferencia de tipo:|parámetro )/.test(trim)) {
      cls = 'te-info'; icon = '◉';
      body = h(trim);
    } else if (/=>/.test(trim)) {
      // condition evaluation line like "  x > 20 => false"
      cls = 'te-if'; icon = ' ';
      const m = trim.match(/^(.+?)\s+=>\s+(.+)/);
      if (m) body = `<span class="cond">${h(m[1])}</span> <span class="arr">⇒</span> <span class="val">${h(m[2])}</span>`;
      else body = h(trim);
    } else {
      body = h(trim);
    }

    const div = document.createElement('div');
    div.className = `te ${cls}`;
    div.innerHTML = `<span class="te-icon">${icon}</span><span class="te-body">${body}</span>`;
    view.appendChild(div);
  });
}

// ── OPT RENDERER ──────────────────────────────────────────────
/*
  Real format from optimizer_visitor.cpp:
    "Resumen de optimizaciones"
    "  Constant folding: N"
    "  Simplificaciones algebraicas: N"
    "  Código muerto eliminado: N"
    "  If con condición constante simplificados: N"
    "  While(false) eliminados: N"
    "  Total: N"
*/
function renderOpt(raw) {
  const view = $('opt-view');
  view.innerHTML = '';

  const lines = raw.split('\n').filter(l => l.trim());

  // Parse the structured output
  const stats = {};
  lines.forEach(line => {
    const m = line.match(/^\s*(.+?):\s*(\d+)\s*$/);
    if (m) stats[m[1].trim()] = parseInt(m[2]);
  });

  // Build stats bar
  const total = stats['Total'] ?? Object.values(stats).reduce((a,b)=>a+b,0);
  const fold  = stats['Constant folding'] ?? 0;
  const alg   = stats['Simplificaciones algebraicas'] ?? 0;
  const dead  = stats['Código muerto eliminado'] ?? 0;
  const br    = stats['If con condición constante simplificados'] ?? 0;
  const lp    = stats['While(false) eliminados'] ?? 0;

  const statsDiv = document.createElement('div');
  statsDiv.className = 'opt-stats';
  statsDiv.innerHTML = `
    <div class="opt-stat-item">
      <span class="opt-stat-label">Total optimizaciones</span>
      <span class="opt-stat-val ${total>0?'green':'blue'}">${total}</span>
    </div>
    <div class="opt-stat-item">
      <span class="opt-stat-label">Constant folding</span>
      <span class="opt-stat-val blue">${fold}</span>
    </div>
    <div class="opt-stat-item">
      <span class="opt-stat-label">Algebraicas</span>
      <span class="opt-stat-val blue">${alg}</span>
    </div>
    <div class="opt-stat-item">
      <span class="opt-stat-label">Código muerto</span>
      <span class="opt-stat-val blue">${dead}</span>
    </div>
    <div class="opt-stat-item">
      <span class="opt-stat-label">Ramas const.</span>
      <span class="opt-stat-val blue">${br}</span>
    </div>
    <div class="opt-stat-item">
      <span class="opt-stat-label">While(false)</span>
      <span class="opt-stat-val blue">${lp}</span>
    </div>
  `;
  view.appendChild(statsDiv);

  // Item cards for each non-zero category
  const items = [
    { key:'Constant folding',                        badge:'ob-fold',   label:'Const Folding' },
    { key:'Simplificaciones algebraicas',            badge:'ob-alg',    label:'Algebraicas'   },
    { key:'Código muerto eliminado',                 badge:'ob-dead',   label:'Código muerto' },
    { key:'If con condición constante simplificados',badge:'ob-branch', label:'Branch const.' },
    { key:'While(false) eliminados',                 badge:'ob-loop',   label:'While(false)'  },
  ];

  items.forEach(({ key, badge, label }) => {
    const n = stats[key] ?? 0;
    const item = document.createElement('div');
    item.className = 'opt-item';
    item.innerHTML = `
      <span class="opt-badge ${badge}">${h(label)}</span>
      <span class="opt-text"><b>${n}</b> optimizaci${n===1?'ón':'ones'} — ${h(key)}</span>
    `;
    view.appendChild(item);
  });

  // If nothing parsed, show raw
  if (Object.keys(stats).length === 0) {
    view.innerHTML = '';
    const pre = document.createElement('pre');
    pre.className = 'code-pre';
    pre.style.flex = '1';
    pre.textContent = raw;
    view.appendChild(pre);
  }
}

// ── ASM COLORIZER ─────────────────────────────────────────────
function colorizeASM(raw) {
  return raw.split('\n').map(line => {
    if (!line.trim()) return '';
    // comment-only line
    if (/^\s*#/.test(line)) return `<span class="ac">${h(line)}</span>`;
    // directive
    if (/^\s*\./.test(line)) return `<span class="ad">${h(line)}</span>`;
    // label (word followed by colon at start)
    if (/^\w[\w.]*:/.test(line.trimStart())) return `<span class="al">${h(line)}</span>`;
    // instruction line
    return line
      .replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;')
      // opcode (first word after whitespace)
      .replace(/^(\s+)([a-z][a-z0-9]*)/, (_, sp, op) => `${h(sp)}<span class="ao">${op}</span>`)
      // registers %rax etc
      .replace(/(%[a-z][a-z0-9]*)/g, '<span class="ar">$1</span>')
      // immediates $N
      .replace(/\$(-?\d+)/g, '<span class="ai">$$$1</span>')
      // memory (N(%reg))
      .replace(/(-?\d+)\((<span class="ar">%[^<]+<\/span>)\)/g,
               '<span class="am">$1($2)</span>')
      // inline comments
      .replace(/(#.*)$/, '<span class="ac">$1</span>');
  }).join('\n');
}

// ── CORE API CALL ─────────────────────────────────────────────
let busy = false;

async function runAction(action, tabId) {
  if (busy) return;
  busy = true;

  const code = codeEditor.value;
  if (!code.trim()) {
    setStatus('El editor está vacío', 'err');
    busy = false;
    return;
  }

  // Switch to the relevant tab
  if (tabId) showTab(tabId);

  // Update UI
  setPipe(action, 's-spin');
  setStatus(`Ejecutando ${action}…`, 'loading');
  setElapsed(null);
  mainRunBtn.classList.add('busy');

  try {
    const res  = await fetch('/api/compile', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ code, action }),
    });
    const data = await res.json();
    const ok   = Boolean(data.ok);
    const ms   = data.elapsed_ms ?? null;

    setPipe(action, ok ? 's-ok' : 's-err');
    setStatus(ok ? `${action} OK` : `Error en ${action}`, ok ? 'ok' : 'err');
    setElapsed(ms);

    // Dispatch to the right renderer
    dispatchResult(action, tabId, data, ok, ms);

    // Collect errors
    if (!ok) {
      const errMsg = [data.error, data.stderr].filter(Boolean).join('\n').trim()
                  || data.stdout || 'Error desconocido';
      addError(action, errMsg);
    }

  } catch (err) {
    setPipe(action, 's-err');
    setStatus('Error de conexión', 'err');
    addError(action, String(err));
  } finally {
    busy = false;
    mainRunBtn.classList.remove('busy');
  }
}

function dispatchResult(action, tabId, data, ok, ms) {
  const stdout = data.stdout || '';
  const stderr = data.stderr || '';
  const errMsg = data.error  || '';

  // Helper: show raw text in a pre panel
  function showPre(preId, hdrId, label, content, extra) {
    showResult(tabId || 'check');
    const hdr = $(hdrId); if (hdr) hdr.innerHTML = makeHdr(ok, label, ms) + (extra||'');
    const pre = $(preId); if (pre) pre.textContent = content || (ok ? '(sin salida)' : stderr || errMsg);
  }

  if (action === 'check') {
    showResult('check');
    const hdr = $('hdr-check');
    if (hdr) hdr.innerHTML = makeHdr(ok, 'Análisis semántico', ms);
    const pre = $('pre-check');
    if (pre) pre.textContent = stdout || (ok ? 'Análisis léxico, sintáctico y semántico exitoso.' : stderr || errMsg);
  }

  else if (action === 'tokens') {
    showResult('tokens');
    $('hdr-tokens').innerHTML = makeHdr(ok, 'Análisis léxico — Tokens', ms);
    if (ok && stdout.trim()) {
      renderTokens(stdout);
    } else {
      $('tok-chips').classList.add('hidden');
      const pre = $('pre-tokens');
      pre.classList.remove('hidden');
      pre.textContent = stderr || errMsg || stdout;
    }
  }

  else if (action === 'ast' || action === 'opt_ast') {
    const tid = action === 'opt_ast' ? 'ast' : 'ast';
    showResult(tid);
    $('hdr-ast').innerHTML = makeHdr(ok, action === 'opt_ast' ? 'AST optimizado' : 'Árbol de Sintaxis Abstracta (AST)', ms);
    const pre = $('pre-ast');
    if (ok && stdout.trim()) pre.innerHTML = colorizeAST(stdout);
    else pre.textContent = stderr || errMsg || stdout;
  }

  else if (action === 'trace') {
    showResult('trace');
    $('hdr-trace').innerHTML = makeHdr(ok, 'Traza educativa', ms);
    if (ok && stdout.trim()) renderTrace(stdout);
    else {
      const view = $('trace-view');
      view.innerHTML = '';
      const pre = document.createElement('pre');
      pre.className = 'code-pre'; pre.style.flex='1';
      pre.textContent = stderr || errMsg || stdout;
      view.appendChild(pre);
    }
  }

  else if (action === 'opt_report') {
    showResult('opt');
    $('hdr-opt').innerHTML = makeHdr(ok, 'Reporte de optimizaciones', ms);
    if (ok && stdout.trim()) renderOpt(stdout);
    else {
      const view = $('opt-view');
      view.innerHTML = '';
      const pre = document.createElement('pre');
      pre.className = 'code-pre'; pre.style.flex='1';
      pre.textContent = stderr || errMsg || stdout;
      view.appendChild(pre);
    }
  }

  else if (action === 'asm') {
    showResult('asm');
    const asmRaw = data.asm || stdout;
    const lines  = asmRaw.split('\n').filter(l=>l.trim()).length;
    $('hdr-asm').innerHTML = makeHdr(ok, 'Ensamblador x86 (optimizado)', ms)
      + `<span class="ms-label">${lines} líneas</span>`;
    const pre = $('pre-asm');
    if (ok && asmRaw.trim()) pre.innerHTML = colorizeASM(asmRaw);
    else pre.textContent = stderr || errMsg;
  }

  else if (action === 'asm_no_opt') {
    showResult('noopt');
    const asmRaw = data.asm || stdout;
    const lines  = asmRaw.split('\n').filter(l=>l.trim()).length;
    $('hdr-noopt').innerHTML = makeHdr(ok, 'Ensamblador x86 (sin optimizar)', ms)
      + `<span class="ms-label">${lines} líneas</span>`;
    const pre = $('pre-noopt');
    if (ok && asmRaw.trim()) pre.innerHTML = colorizeASM(asmRaw);
    else pre.textContent = stderr || errMsg;
  }

  else if (action === 'run') {
    showResult('run');
    $('hdr-run').innerHTML = makeHdr(ok, 'Ejecución del programa', ms);
    const pre = $('pre-run');
    pre.textContent = stdout || (ok ? '(sin salida)' : stderr || errMsg);

    // If ASM was also generated, populate that tab too (silently)
    if (data.asm) {
      showResult('asm');
      const lines = data.asm.split('\n').filter(l=>l.trim()).length;
      $('hdr-asm').innerHTML = makeHdr(true, 'Ensamblador x86 (generado en ejecución)', null)
        + `<span class="ms-label">${lines} líneas</span>`;
      $('pre-asm').innerHTML = colorizeASM(data.asm);
    }
  }
}

// ── PIPELINE BUTTON CLICKS ────────────────────────────────────
document.querySelectorAll('.ps').forEach(btn => {
  btn.addEventListener('click', () => {
    const action = btn.dataset.action;
    const tab    = btn.dataset.tab;
    runAction(action, tab);
  });
});

// ── TOPBAR: SAMPLE / RUN / CLEAR / COPY ──────────────────────
$('loadSampleBtn').addEventListener('click', () => {
  codeEditor.value = SAMPLE;
  updateLines();
  updateCursor();
  setStatus('Ejemplo cargado', 'ok');
  setTimeout(() => setStatus('Listo'), 2000);
});

mainRunBtn.addEventListener('click', () => runAction('run', 'run'));

$('btnClear').addEventListener('click', clearAllResults);

$('btnCopy').addEventListener('click', () => {
  const activePanel = document.querySelector('.panel.active');
  if (!activePanel) return;
  const pre = activePanel.querySelector('pre');
  const text = pre ? pre.textContent : '';
  if (!text) return;
  navigator.clipboard.writeText(text).then(() => {
    const btn = $('btnCopy');
    btn.textContent = '✓';
    setTimeout(() => { btn.textContent = '⎘'; }, 1500);
  });
});

// ── EDITOR: LINE NUMBERS + CURSOR ────────────────────────────
function updateLines() {
  const n = (codeEditor.value.match(/\n/g) || []).length + 1;
  lineNums.textContent = Array.from({length:n},(_,i)=>i+1).join('\n');
}

function syncLinesScroll() {
  lineNums.scrollTop = codeEditor.scrollTop;
}

function updateCursor() {
  const text = codeEditor.value;
  const pos  = codeEditor.selectionStart;
  const before = text.slice(0, pos).split('\n');
  const line = before.length;
  const col  = before[before.length-1].length + 1;
  cursorPos.textContent = `Línea ${line}, Col ${col}`;
  charCount.textContent = `${text.length} chars`;
}

codeEditor.addEventListener('input',  () => { updateLines(); updateCursor(); });
codeEditor.addEventListener('scroll', syncLinesScroll);
codeEditor.addEventListener('click',  updateCursor);
codeEditor.addEventListener('keyup',  updateCursor);
codeEditor.addEventListener('keydown', e => {
  if (e.key === 'Tab') {
    e.preventDefault();
    const s = codeEditor.selectionStart;
    const ee = codeEditor.selectionEnd;
    codeEditor.value = codeEditor.value.slice(0,s) + '    ' + codeEditor.value.slice(ee);
    codeEditor.selectionStart = codeEditor.selectionEnd = s + 4;
    updateLines();
  }
});

// ── RESIZE HANDLE ─────────────────────────────────────────────
(function() {
  const handle = $('resizer');
  const left   = $('paneEditor');
  let drag = false, startX = 0, startW = 0;
  handle.addEventListener('mousedown', e => {
    drag = true; startX = e.clientX; startW = left.offsetWidth;
    handle.classList.add('drag');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  });
  document.addEventListener('mousemove', e => {
    if (!drag) return;
    const newW = Math.max(240, Math.min(startW + e.clientX - startX, window.innerWidth - 300));
    left.style.width = newW + 'px';
  });
  document.addEventListener('mouseup', () => {
    if (!drag) return;
    drag = false;
    handle.classList.remove('drag');
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
  });
})();

// ── HEALTH CHECK ──────────────────────────────────────────────
(async function checkHealth() {
  try {
    const res  = await fetch('/api/health');
    const data = await res.json();
    const dot  = $('healthDot');
    dot.className = `health-dot ${data.ok && data.exists ? 'ok' : 'fail'}`;
    dot.title = data.exists
      ? `Compilador listo: ${data.compiler}`
      : `Compilador no encontrado: ${data.compiler}`;
  } catch {
    $('healthDot').className = 'health-dot fail';
    $('healthDot').title = 'Servidor no disponible';
  }
})();

// ── INIT ──────────────────────────────────────────────────────
codeEditor.value = SAMPLE;
updateLines();
updateCursor();