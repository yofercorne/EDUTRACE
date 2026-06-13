/* ================================================================
   EduTrace App — app.js
   
   Compiler output formats (from reading source):
     tokens:     "TypeName 'lexeme' (line:col)"  e.g. Fun 'fun' (1:1)
     ast:        2-space indented text, via ASTPrinter
     check:      "Análisis léxico, sintáctico y semántico exitoso."
     trace:      lines with "=== EduTrace...", "print:", "Variable X cambió:", etc.
     opt_report: "Resumen de optimizaciones\n  Constant folding: N\n..."
     asm/asm_no_opt: data.asm field (file written by compiler, read by server)
     run:        data.stdout = program stdout, data.asm = asm generated
================================================================ */
'use strict';

// ── DOM SHORTCUTS ─────────────────────────────────────────────
const $ = id => document.getElementById(id);

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
const tabHint    = $('tabHint');

// ── SAMPLE CODE ───────────────────────────────────────────────
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

// ── TAB SYSTEM ────────────────────────────────────────────────
// Tab config: maps tabId → { action, hint }
const TAB_CONFIG = {
  check:  { action: 'check',      hint: 'Semántica — verifica tipos y declaraciones.' },
  tokens: { action: 'tokens',     hint: 'Tokens — resultado del análisis léxico. Cada símbolo reconocido.' },
  ast:    { action: 'ast',        hint: 'AST — árbol de sintaxis abstracta: la estructura jerárquica del programa.' },
  trace:  { action: 'trace',      hint: 'Trace — explicación paso a paso de lo que ejecuta el programa.' },
  opt:    { action: 'opt_report', hint: 'Optimización — transfor­maciones aplicadas por el compilador (con comparación ASM).' },
  asm:    { action: 'asm',        hint: 'ASM x86 — código ensamblador optimizado generado por el compilador.' },
  run:    { action: 'run',        hint: 'Ejecutar — compila el programa y muestra su salida real.' },
  err:    { action: null,         hint: 'Errores — todos los errores encontrados en esta sesión.' },
};

function showTab(tabId) {
  document.querySelectorAll('.tab').forEach(t =>
    t.classList.toggle('active', t.dataset.tab === tabId));
  document.querySelectorAll('.panel').forEach(p =>
    p.classList.toggle('active', p.id === `panel-${tabId}`));
  const cfg = TAB_CONFIG[tabId];
  if (tabHint) tabHint.textContent = cfg ? cfg.hint : '';
}

// Tabs: clicking fires the action for that tab
document.querySelectorAll('.tab').forEach(btn => {
  btn.addEventListener('click', () => {
    const tabId = btn.dataset.tab;
    showTab(tabId);
    const cfg = TAB_CONFIG[tabId];
    if (cfg && cfg.action && !btn.hasAttribute('data-no-action')) {
      runAction(cfg.action, tabId);
    }
  });
});

// ── PIPELINE STATUS INDICATORS (visual only) ──────────────────
const PIPE_MAP = {
  check:      'ps-check',
  tokens:     'ps-tokens',
  ast:        'ps-ast',
  opt_ast:    'ps-ast',
  trace:      'ps-trace',
  opt_report: 'ps-opt',
  asm:        'ps-asm',
  asm_no_opt: 'ps-asm',
  run:        'ps-run',
};

function setPipe(action, state) {
  const elId = PIPE_MAP[action];
  if (!elId) return;
  const el = $(elId);
  if (!el) return;
  el.classList.remove('s-ok', 's-err', 's-run');
  if (state) el.classList.add(state);
}

// ── STATUS BAR ────────────────────────────────────────────────
function setStatus(msg, state = '') {
  sbMsg.textContent = msg;
  sbMsg.className = `sb-msg ${state}`;
  sbDot.className = `sb-dot ${
    state === 'ok' ? 'ok' : state === 'err' ? 'err' : state === 'loading' ? 'spin' : ''
  }`;
}
function setElapsed(ms) {
  sbElapsed.textContent = ms == null ? '' : ms < 1000 ? `${ms}ms` : `${(ms / 1000).toFixed(2)}s`;
}

// ── UTILITY ───────────────────────────────────────────────────
function h(s) {
  return String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}
function showEmpty(id) {
  const es = $(`es-${id}`); if (es) es.style.display = '';
  const res = $(`res-${id}`); if (res) res.classList.add('hidden');
}
function showResult(id) {
  const es = $(`es-${id}`); if (es) es.style.display = 'none';
  const res = $(`res-${id}`); if (res) res.classList.remove('hidden');
}
function makeHdr(ok, label, ms) {
  const tag = ok
    ? `<span class="tag tag-ok">✓ OK</span>`
    : `<span class="tag tag-err">✗ Error</span>`;
  const time = ms != null ? `<span class="ms-label">${ms < 1000 ? ms + 'ms' : (ms / 1000).toFixed(2) + 's'}</span>` : '';
  return `${tag}<span>${h(label)}</span>${time}`;
}

// ── ERROR PANEL ───────────────────────────────────────────────
let errTotal = 0;

function addError(source, msg) {
  if (!msg || !msg.trim()) return;
  errTotal++;
  errBadge.hidden = false;
  errBadge.textContent = errTotal;
  showResult('err');
  const item = document.createElement('div');
  item.className = 'err-item';
  item.innerHTML = `<div class="err-src">${h(source)}</div><div class="err-msg">${h(msg.trim())}</div>`;
  $('err-list').appendChild(item);
}

function clearAllResults() {
  ['check', 'tokens', 'ast', 'trace', 'opt', 'asm', 'run', 'err'].forEach(showEmpty);
  ['pre-check', 'pre-tokens', 'pre-ast', 'pre-asm', 'pre-run'].forEach(id => {
    const el = $(id); if (el) { el.innerHTML = ''; el.textContent = ''; }
  });
  ['tok-chips', 'tok-tbody', 'trace-view', 'opt-view', 'err-list', 'ast-tree'].forEach(id => {
    const el = $(id); if (el) el.innerHTML = '';
  });
  ['hdr-check', 'hdr-tokens', 'hdr-ast', 'hdr-trace', 'hdr-opt', 'hdr-asm', 'hdr-run'].forEach(id => {
    const el = $(id); if (el) el.innerHTML = '';
  });
  errTotal = 0;
  errBadge.hidden = true;
  errBadge.textContent = '0';
  Object.keys(PIPE_MAP).forEach(a => setPipe(a, ''));
  setStatus('Listo — selecciona una pestaña para ejecutar');
  setElapsed(null);
  // reset token view
  setTokenView('chips', true);
  // reset ast view
  setAstView('tree', true);
}

// ══════════════════════════════════════════════════════════════
// ── TOKENS ───────────────────────────────────────────────────
// Format: "TypeName 'lexeme' (line:col)"
// ══════════════════════════════════════════════════════════════

const KW_SET   = new Set(['Fun','Var','Let','If','Else','While','For','Return','Print','New','Delete','Lambda','True','False']);
const TYPE_SET  = new Set(['Int','Bool','Char','StringType','Void']);
const PUNCT_SET = new Set(['LeftParen','RightParen','LeftBrace','RightBrace','LeftBracket','RightBracket','Comma','Dot','Semicolon','Colon']);
const OP_SET    = new Set(['Plus','Minus','Star','Slash','Percent','Ampersand','Bang','BangEqual','Equal','EqualEqual','Greater','GreaterEqual','Less','LessEqual','AndAnd','OrOr']);
const EDU_SET   = new Set(['Trace','Variables','Loop','Branch','Stack','Recursion','Array','Memory']);

function tokenCategory(typeName) {
  if (typeName === 'Identifier')    return { cls: 'tk-id',   cat: 'identifier' };
  if (typeName === 'Number')        return { cls: 'tk-num',  cat: 'literal' };
  if (typeName === 'StringLiteral') return { cls: 'tk-str',  cat: 'literal' };
  if (typeName === 'CharLiteral')   return { cls: 'tk-str',  cat: 'literal' };
  if (typeName === 'EndOfFile')     return { cls: 'tk-eof',  cat: 'eof' };
  if (KW_SET.has(typeName))         return { cls: 'tk-kw',   cat: 'keyword' };
  if (TYPE_SET.has(typeName))       return { cls: 'tk-type', cat: 'type' };
  if (PUNCT_SET.has(typeName))      return { cls: 'tk-pun',  cat: 'punctuation' };
  if (OP_SET.has(typeName))         return { cls: 'tk-op',   cat: 'operator' };
  if (EDU_SET.has(typeName))        return { cls: 'tk-edu',  cat: 'trace' };
  return { cls: 'tk-pun', cat: 'other' };
}

// Parse raw token output into structured array
function parseTokens(raw) {
  // Real format: TypeName 'lexeme' (line:col)
  const RE = /^([A-Za-z]+)\s+'(.*)'\s+\((\d+):(\d+)\)\s*$/;
  const result = [];
  raw.split('\n').forEach(line => {
    const m = line.match(RE);
    if (!m) return;
    const [, type, lexeme, lineN, col] = m;
    const { cls, cat } = tokenCategory(type);
    result.push({ type, lexeme, line: parseInt(lineN), col: parseInt(col), cls, cat });
  });
  return result;
}

let _tokenData = []; // cached for view switching
let _tokenView = 'chips';

function setTokenView(view, silent) {
  _tokenView = view;
  const chips = $('tok-chips');
  const tableWrap = $('tok-table-wrap');
  const pre = $('pre-tokens');
  const togC = $('togChips');
  const togT = $('togTable');

  if (chips) chips.classList.toggle('hidden', view !== 'chips');
  if (tableWrap) tableWrap.classList.toggle('hidden', view !== 'table');
  if (pre) pre.classList.add('hidden');
  if (togC) togC.classList.toggle('active', view === 'chips');
  if (togT) togT.classList.toggle('active', view === 'table');

  if (!silent && _tokenData.length) {
    if (view === 'chips') renderTokenChips(_tokenData);
    else renderTokenTable(_tokenData);
  }
}

function renderTokenChips(tokens) {
  const el = $('tok-chips');
  if (!el) return;
  el.innerHTML = '';
  tokens.forEach(({ type, lexeme, line, col, cls }) => {
    const chip = document.createElement('span');
    chip.className = `tk ${cls}`;
    chip.title = `${type} @ ${line}:${col}`;
    const displayLex = lexeme.length <= 22 ? lexeme : lexeme.slice(0, 20) + '…';
    const isSymbol = cls === 'tk-pun' || cls === 'tk-op';
    if (isSymbol) {
      chip.innerHTML = `<span>${h(type)}</span>${lexeme ? `<span class="tk-lex">'${h(displayLex)}'</span>` : ''}`;
    } else {
      chip.innerHTML = `<span>${h(displayLex || type)}</span><span class="tk-lex">${h(type)}</span>`;
    }
    el.appendChild(chip);
  });
}

function renderTokenTable(tokens) {
  const tbody = $('tok-tbody');
  if (!tbody) return;
  tbody.innerHTML = '';
  tokens.forEach(({ type, lexeme, line, col, cls, cat }, i) => {
    const tc = cls.replace('tk-', 'tt-');
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td>${i + 1}</td>
      <td class="${tc}">${h(type)}</td>
      <td class="tt-lex">${h(lexeme)}</td>
      <td>${line}</td>
      <td>${col}</td>
      <td class="tt-cat ${tc}">${h(cat)}</td>
    `;
    tbody.appendChild(tr);
  });
}

function renderTokens(raw) {
  _tokenData = parseTokens(raw);
  if (_tokenData.length === 0) {
    // fallback: show raw
    $('tok-chips').classList.add('hidden');
    $('tok-table-wrap').classList.add('hidden');
    const pre = $('pre-tokens');
    pre.classList.remove('hidden');
    pre.textContent = raw;
    return;
  }
  if (_tokenView === 'table') renderTokenTable(_tokenData);
  else renderTokenChips(_tokenData);
}

// ══════════════════════════════════════════════════════════════
// ── AST TREE ─────────────────────────────────────────────────
// Format: 2-space indented text from ASTPrinter in main.cpp
// ══════════════════════════════════════════════════════════════

let _astRaw = '';
let _astView = 'tree';

function setAstView(view, silent) {
  _astView = view;
  const treeWrap = $('ast-tree-wrap');
  const pre      = $('pre-ast');
  const togTree  = $('togAstTree');
  const togRaw   = $('togAstRaw');
  if (treeWrap) treeWrap.classList.toggle('hidden', view !== 'tree');
  if (pre)      pre.classList.toggle('hidden', view !== 'raw');
  if (togTree)  togTree.classList.toggle('active', view === 'tree');
  if (togRaw)   togRaw.classList.toggle('active', view === 'raw');
  if (!silent && _astRaw) {
    if (view === 'tree') renderASTTree(_astRaw);
    else renderASTRaw(_astRaw);
  }
}

function astNodeClass(label) {
  if (/^Program/.test(label))      return 'ak-prog';
  if (/^Function|^Struct/.test(label)) return 'ak-fn';
  if (/^Param/.test(label))        return 'ak-param';
  if (/^Block/.test(label))        return 'ak-blk';
  if (/^LetDecl|^VarDecl|^Field/.test(label)) return 'ak-decl';
  if (/^(If|While|For|Return|Print|Assign|ArrayAssign|PointerAssign|FieldAssign|Trace|ExprStmt|Delete)/.test(label)) return 'ak-stmt';
  if (/^(Binary|Unary|Call|ArrayAccess|FieldAccess|Lambda|New)/.test(label)) return 'ak-expr';
  if (/^Id\s/.test(label))         return 'ak-id';
  if (/^String\s/.test(label))     return 'ak-str';
  if (/^(Int|Bool|Char)\s/.test(label)) return 'ak-lit';
  return 'ak-blk';
}

// Parse indented text into a tree of {label, children}
function parseASTIndent(raw) {
  const lines = raw.split('\n').filter(l => l.trimEnd());
  const root = { label: 'root', children: [], depth: -1 };
  const stack = [root];

  lines.forEach(line => {
    const spaces = line.match(/^(\s*)/)[1].length;
    // ASTPrinter uses 2 spaces per level
    const depth = Math.floor(spaces / 2);
    const label = line.trim();
    if (!label) return;
    const node = { label, children: [], depth };
    // Pop stack until we find the parent
    while (stack.length > 1 && stack[stack.length - 1].depth >= depth) stack.pop();
    stack[stack.length - 1].children.push(node);
    stack.push(node);
  });

  return root.children;
}

function buildASTHtml(nodes, isLast = []) {
  if (!nodes || !nodes.length) return '';
  return nodes.map((node, i) => {
    const last = i === nodes.length - 1;
    const hasChildren = node.children && node.children.length > 0;
    const cls = astNodeClass(node.label);
    const nodeId = 'an' + Math.random().toString(36).slice(2, 8);

    const toggleBtn = hasChildren
      ? `<button class="ast-toggle" onclick="toggleASTNode('${nodeId}')" title="Colapsar/Expandir">−</button>`
      : `<span class="ast-toggle-spacer"></span>`;

    const labelHtml = `<span class="ast-label ${cls}">${h(node.label)}</span>`;

    const childrenHtml = hasChildren
      ? `<div class="ast-children" id="${nodeId}">${buildASTHtml(node.children)}</div>`
      : '';

    return `<div class="ast-node-wrap">
      <div class="ast-node-row">
        ${toggleBtn}
        ${labelHtml}
      </div>
      ${childrenHtml}
    </div>`;
  }).join('');
}

function renderASTTree(raw) {
  const container = $('ast-tree');
  if (!container) return;
  const nodes = parseASTIndent(raw);
  container.innerHTML = buildASTHtml(nodes);
}

function renderASTRaw(raw) {
  const pre = $('pre-ast');
  if (!pre) return;
  // Color the raw text
  pre.innerHTML = raw.split('\n').map(line => {
    const indent = line.match(/^(\s*)/)[1];
    const rest   = line.slice(indent.length);
    if (!rest) return '';
    const cls = astNodeClass(rest);
    return `${h(indent)}<span class="${cls}">${h(rest)}</span>`;
  }).join('\n');
}

function toggleASTNode(id) {
  const el = $(id);
  if (!el) return;
  const btn = el.previousElementSibling && el.previousElementSibling.querySelector('.ast-toggle');
  const collapsed = el.classList.toggle('collapsed');
  if (btn) btn.textContent = collapsed ? '+' : '−';
}

function astExpandAll(expand) {
  document.querySelectorAll('.ast-children').forEach(el => {
    el.classList.toggle('collapsed', !expand);
  });
  document.querySelectorAll('.ast-toggle').forEach(btn => {
    btn.textContent = expand ? '−' : '+';
  });
}

// ══════════════════════════════════════════════════════════════
// ── TRACE ─────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════

function renderTrace(raw) {
  const view = $('trace-view');
  view.innerHTML = '';

  raw.split('\n').forEach(line => {
    if (!line.trim()) return;
    const trim = line.trimStart();
    let cls = 'te-info', icon = '·', body = '';

    if (/^===/.test(trim)) {
      cls = 'te-sep'; icon = '━';
      body = `<b>${h(trim.replace(/===/g, '').trim())}</b>`;
    } else if (/^Entrando a (función|lambda):/.test(trim)) {
      cls = 'te-fn'; icon = '→';
      body = `<b>${h(trim)}</b>`;
    } else if (/^Retorna .+ desde /.test(trim)) {
      cls = 'te-ret'; icon = '⏎';
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
      body = `Rama <b class="${branch === 'THEN' ? 'ok' : 'arr'}">${branch}</b>`;
    } else if (/^(Fin del ciclo|While: condición)/.test(trim)) {
      cls = 'te-loop'; icon = '↻';
      body = h(trim);
    } else if (/^Iteración \d+/.test(trim)) {
      cls = 'te-loop'; icon = '↻';
      body = `<b>${h(trim)}</b>`;
    } else if (/=>/.test(trim)) {
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

// ══════════════════════════════════════════════════════════════
// ── OPTIMIZACIÓN ─────────────────────────────────────────────
// Uses opt_report + fetches asm + asm_no_opt for comparison
// ══════════════════════════════════════════════════════════════

/*
  Real format from optimizer_visitor.cpp:
    Resumen de optimizaciones
      Constant folding: N
      Simplificaciones algebraicas: N
      Código muerto eliminado: N
      If con condición constante simplificados: N
      While(false) eliminados: N
      Total: N
*/

// Explanation texts for each optimization type
const OPT_EXPLAIN = {
  'Constant folding':
    'Expresiones que solo contienen constantes se calculan en tiempo de compilación. Por ejemplo: 2 + 3 * 4 → 14.',
  'Simplificaciones algebraicas':
    'Identidades algebraicas como x * 1 → x, x + 0 → x o x * 0 → 0 se simplifican automáticamente.',
  'Código muerto eliminado':
    'Sentencias que nunca se ejecutan (por ejemplo, después de un return, o dentro de un if(false)) se eliminan del AST.',
  'If con condición constante simplificados':
    'Si la condición del if es siempre true o false, se elimina la rama muerta. Ej: if(true){...}else{...} → solo el then.',
  'While(false) eliminados':
    'Un ciclo while cuya condición siempre es false nunca se ejecuta, así que se elimina completamente del código generado.',
};

function parseOptReport(raw) {
  const stats = {};
  raw.split('\n').forEach(line => {
    const m = line.match(/^\s*(.+?):\s*(\d+)\s*$/);
    if (m) stats[m[1].trim()] = parseInt(m[2]);
  });
  return stats;
}

async function runOptFull() {
  showTab('opt');
  const code = codeEditor.value.trim();
  if (!code) { setStatus('El editor está vacío', 'err'); return; }

  const view = $('opt-view');
  view.innerHTML = `<div class="opt-loading"><div class="opt-spinner"></div>Cargando optimizaciones…</div>`;
  showResult('opt');
  setStatus('Analizando optimizaciones…', 'loading');
  setElapsed(null);
  setPipe('opt_report', 's-run');
  mainRunBtn.classList.add('busy');

  try {
    // Run all 3 in parallel: opt_report, asm (optimized), asm_no_opt
    const [repRes, asmRes, noOptRes] = await Promise.all([
      apiCompile(code, 'opt_report'),
      apiCompile(code, 'asm'),
      apiCompile(code, 'asm_no_opt'),
    ]);

    const ok = repRes.ok;
    const ms = repRes.elapsed_ms ?? null;
    setPipe('opt_report', ok ? 's-ok' : 's-err');
    setStatus(ok ? 'Optimización OK' : 'Error en optimización', ok ? 'ok' : 'err');
    setElapsed(ms);
    $('hdr-opt').innerHTML = makeHdr(ok, 'Reporte de optimizaciones', ms);

    if (!ok) {
      view.innerHTML = '';
      const pre = document.createElement('pre');
      pre.className = 'code-pre'; pre.style.flex = '1';
      pre.textContent = repRes.stderr || repRes.error || repRes.stdout;
      view.appendChild(pre);
      addError('opt_report', repRes.stderr || repRes.error || '');
      return;
    }

    renderOptFull(repRes, asmRes, noOptRes);

  } catch (err) {
    setPipe('opt_report', 's-err');
    setStatus('Error de conexión', 'err');
    addError('opt_report', String(err));
    view.innerHTML = '';
  } finally {
    mainRunBtn.classList.remove('busy');
    busy = false;
  }
}

function renderOptFull(repData, asmData, noOptData) {
  const view = $('opt-view');
  view.innerHTML = '';

  const stats = parseOptReport(repData.stdout || '');
  const total = stats['Total'] ?? 0;
  const fold  = stats['Constant folding'] ?? 0;
  const alg   = stats['Simplificaciones algebraicas'] ?? 0;
  const dead  = stats['Código muerto eliminado'] ?? 0;
  const br    = stats['If con condición constante simplificados'] ?? 0;
  const lp    = stats['While(false) eliminados'] ?? 0;

  const asmOpt   = asmData.asm   || asmData.stdout   || '';
  const asmNoOpt = noOptData.asm || noOptData.stdout || '';

  const asmOptLines   = asmOpt.split('\n').filter(l => l.trim()).length;
  const asmNoOptLines = asmNoOpt.split('\n').filter(l => l.trim()).length;
  const reduction     = asmNoOptLines > 0
    ? (((asmNoOptLines - asmOptLines) / asmNoOptLines) * 100).toFixed(1)
    : '0.0';

  // ── SECTION 1: Summary stats ──
  const sec1 = document.createElement('div');
  sec1.className = 'opt-section';
  sec1.innerHTML = `
    <div class="opt-section-title">Resumen</div>
    <div class="opt-stats-grid">
      <div class="opt-stat-card">
        <div class="opt-stat-label">Total optimizaciones</div>
        <div class="opt-stat-val ${total > 0 ? 'green' : 'blue'}">${total}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">Const. folding</div>
        <div class="opt-stat-val blue">${fold}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">Algebraicas</div>
        <div class="opt-stat-val blue">${alg}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">Código muerto</div>
        <div class="opt-stat-val blue">${dead}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">Ramas const.</div>
        <div class="opt-stat-val blue">${br}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">While(false)</div>
        <div class="opt-stat-val blue">${lp}</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">ASM sin opt</div>
        <div class="opt-stat-val warn">${asmNoOptLines} líneas</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">ASM optimizado</div>
        <div class="opt-stat-val green">${asmOptLines} líneas</div>
      </div>
      <div class="opt-stat-card">
        <div class="opt-stat-label">Reducción ASM</div>
        <div class="opt-stat-val green">↓ ${reduction}%</div>
      </div>
    </div>
  `;
  view.appendChild(sec1);

  // ── SECTION 2: ASM comparison ──
  if (asmOpt || asmNoOpt) {
    const sec2 = document.createElement('div');
    sec2.className = 'opt-section';
    sec2.innerHTML = `
      <div class="opt-section-title">Comparación ASM</div>
      <div class="opt-asm-compare">
        <div class="opt-asm-col">
          <div class="opt-asm-col-title">
            Sin optimizar
            <span class="lc">${asmNoOptLines} líneas</span>
          </div>
          <pre class="opt-asm-pre">${colorizeASM(asmNoOpt)}</pre>
        </div>
        <div class="opt-asm-col">
          <div class="opt-asm-col-title">
            Optimizado
            <span class="lc">${asmOptLines} líneas</span>
          </div>
          <pre class="opt-asm-pre">${colorizeASM(asmOpt)}</pre>
        </div>
      </div>
    `;
    view.appendChild(sec2);
  }

  // ── SECTION 3: Detail cards per optimization type ──
  const items = [
    { key: 'Constant folding',                         count: fold, badge: 'ob-fold' },
    { key: 'Simplificaciones algebraicas',             count: alg,  badge: 'ob-alg'  },
    { key: 'Código muerto eliminado',                  count: dead, badge: 'ob-dead' },
    { key: 'If con condición constante simplificados', count: br,   badge: 'ob-branch'},
    { key: 'While(false) eliminados',                  count: lp,   badge: 'ob-loop' },
  ];

  const nonZero = items.filter(it => it.count > 0);
  if (nonZero.length > 0) {
    const sec3 = document.createElement('div');
    sec3.className = 'opt-section';
    sec3.innerHTML = `<div class="opt-section-title">Optimizaciones aplicadas</div>`;
    const list = document.createElement('div');
    list.className = 'opt-detail-list';

    nonZero.forEach(({ key, count, badge }) => {
      const explain = OPT_EXPLAIN[key] || '';
      const card = document.createElement('div');
      card.className = 'opt-card';
      card.innerHTML = `
        <div class="opt-card-hdr">
          <span class="opt-badge ${badge}">${h(key)}</span>
          <span class="opt-card-count">${count}×</span>
        </div>
        <div class="opt-card-body">
          <div class="opt-card-explain">${h(explain)}</div>
        </div>
      `;
      list.appendChild(card);
    });

    sec3.appendChild(list);
    view.appendChild(sec3);
  } else {
    const sec3 = document.createElement('div');
    sec3.className = 'opt-section';
    sec3.innerHTML = `
      <div class="opt-section-title">Optimizaciones aplicadas</div>
      <p style="color:var(--t3);font-size:12px">No se aplicaron optimizaciones en este programa.</p>
    `;
    view.appendChild(sec3);
  }
}

// ══════════════════════════════════════════════════════════════
// ── ASM COLORIZER ─────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════

function colorizeASM(raw) {
  if (!raw) return '';
  return raw.split('\n').map(line => {
    if (!line.trim()) return '';
    if (/^\s*#/.test(line))            return `<span class="ac">${h(line)}</span>`;
    if (/^\s*\./.test(line))           return `<span class="ad">${h(line)}</span>`;
    if (/^\s*\w[\w.]*:/.test(line))    return `<span class="al">${h(line)}</span>`;
    return line
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/^(\s+)([a-z][a-z0-9]*)/, (_, sp, op) => `${sp}<span class="ao">${op}</span>`)
      .replace(/(%[a-z][a-z0-9]*)/g, '<span class="ar">$1</span>')
      .replace(/\$(-?\d+)/g, '<span class="ai">$$$1</span>')
      .replace(/(-?\d+)\((<span class="ar">%[^<]+<\/span>)\)/g, '<span class="am">$1($2)</span>')
      .replace(/(#.*)$/, '<span class="ac">$1</span>');
  }).join('\n');
}

// ══════════════════════════════════════════════════════════════
// ── CORE API ──────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════

let busy = false;

async function apiCompile(code, action) {
  const res = await fetch('/api/compile', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ code, action }),
  });
  return await res.json();
}

async function runAction(action, tabId) {
  if (busy) return;
  // opt_report gets its own full handler
  if (action === 'opt_report') { await runOptFull(); return; }

  busy = true;

  const code = codeEditor.value.trim();
  if (!code) {
    setStatus('El editor está vacío', 'err');
    busy = false;
    return;
  }

  if (tabId) showTab(tabId);
  setPipe(action, 's-run');
  setStatus(`Ejecutando ${action}…`, 'loading');
  setElapsed(null);
  mainRunBtn.classList.add('busy');

  try {
    const data = await apiCompile(code, action);
    const ok = Boolean(data.ok);
    const ms = data.elapsed_ms ?? null;

    setPipe(action, ok ? 's-ok' : 's-err');
    setStatus(ok ? `${action} completado` : `Error en ${action}`, ok ? 'ok' : 'err');
    setElapsed(ms);

    dispatchResult(action, tabId, data, ok, ms);

    if (!ok) {
      const errMsg = [data.error, data.stderr].filter(Boolean).join('\n').trim() || data.stdout || 'Error desconocido';
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
  const fallback = stderr || errMsg || stdout;

  if (action === 'check') {
    showResult('check');
    $('hdr-check').innerHTML = makeHdr(ok, 'Análisis semántico', ms);
    $('pre-check').textContent = stdout || (ok ? 'Análisis léxico, sintáctico y semántico exitoso.' : fallback);
  }

  else if (action === 'tokens') {
    showResult('tokens');
    $('hdr-tokens').innerHTML = makeHdr(ok, 'Análisis léxico — Tokens', ms);
    if (ok && stdout.trim()) {
      renderTokens(stdout);
    } else {
      $('tok-chips').classList.add('hidden');
      $('tok-table-wrap').classList.add('hidden');
      const pre = $('pre-tokens');
      pre.classList.remove('hidden');
      pre.textContent = fallback;
    }
  }

  else if (action === 'ast' || action === 'opt_ast') {
    showResult('ast');
    const label = action === 'opt_ast' ? 'AST optimizado' : 'Árbol de Sintaxis Abstracta';
    $('hdr-ast').innerHTML = makeHdr(ok, label, ms);
    if (ok && stdout.trim()) {
      _astRaw = stdout;
      if (_astView === 'tree') renderASTTree(stdout);
      else renderASTRaw(stdout);
    } else {
      $('ast-tree-wrap').classList.add('hidden');
      const pre = $('pre-ast');
      pre.classList.remove('hidden');
      pre.textContent = fallback;
    }
  }

  else if (action === 'trace') {
    showResult('trace');
    $('hdr-trace').innerHTML = makeHdr(ok, 'Traza educativa', ms);
    if (ok && stdout.trim()) {
      renderTrace(stdout);
    } else {
      const view = $('trace-view');
      view.innerHTML = '';
      const pre = document.createElement('pre');
      pre.className = 'code-pre'; pre.style.flex = '1';
      pre.textContent = fallback;
      view.appendChild(pre);
    }
  }

  else if (action === 'asm' || action === 'asm_no_opt') {
    showResult('asm');
    const asmRaw = data.asm || stdout;
    const lines  = asmRaw.split('\n').filter(l => l.trim()).length;
    const label  = action === 'asm' ? 'Ensamblador x86 (optimizado)' : 'Ensamblador x86 (sin optimizar)';
    $('hdr-asm').innerHTML = makeHdr(ok, label, ms) + `<span class="ms-label">${lines} líneas</span>`;
    if (ok && asmRaw.trim()) $('pre-asm').innerHTML = colorizeASM(asmRaw);
    else $('pre-asm').textContent = fallback;
  }

  else if (action === 'run') {
    showResult('run');
    $('hdr-run').innerHTML = makeHdr(ok, 'Salida del programa', ms);
    $('pre-run').textContent = stdout || (ok ? '(sin salida)' : fallback);
    // silently fill ASM tab too
    if (data.asm) {
      showResult('asm');
      const lines = data.asm.split('\n').filter(l => l.trim()).length;
      $('hdr-asm').innerHTML = makeHdr(true, 'Ensamblador x86 (generado en ejecución)', null)
        + `<span class="ms-label">${lines} líneas</span>`;
      $('pre-asm').innerHTML = colorizeASM(data.asm);
    }
  }
}

// ══════════════════════════════════════════════════════════════
// ── EVENT LISTENERS ───────────────────────────────────────────
// ══════════════════════════════════════════════════════════════

$('loadSampleBtn').addEventListener('click', () => {
  codeEditor.value = SAMPLE;
  updateLines(); updateCursor();
  setStatus('Ejemplo cargado', 'ok');
  setTimeout(() => setStatus('Listo — selecciona una pestaña para ejecutar'), 2000);
});

mainRunBtn.addEventListener('click', () => {
  showTab('run');
  runAction('run', 'run');
});

$('btnClear').addEventListener('click', clearAllResults);

$('btnCopy').addEventListener('click', () => {
  const activePanel = document.querySelector('.panel.active');
  if (!activePanel) return;
  // Try to get text from visible pre first
  const visiblePre = Array.from(activePanel.querySelectorAll('pre'))
    .find(p => !p.closest('.hidden'));
  const text = visiblePre ? visiblePre.textContent : '';
  if (!text.trim()) return;
  navigator.clipboard.writeText(text).then(() => {
    const btn = $('btnCopy');
    btn.textContent = '✓';
    setTimeout(() => { btn.textContent = '⎘'; }, 1500);
  });
});

// ── EDITOR ────────────────────────────────────────────────────
function updateLines() {
  const n = (codeEditor.value.match(/\n/g) || []).length + 1;
  lineNums.textContent = Array.from({ length: n }, (_, i) => i + 1).join('\n');
}
function syncLinesScroll() { lineNums.scrollTop = codeEditor.scrollTop; }
function updateCursor() {
  const text = codeEditor.value;
  const pos  = codeEditor.selectionStart;
  const before = text.slice(0, pos).split('\n');
  cursorPos.textContent = `Línea ${before.length}, Col ${before[before.length - 1].length + 1}`;
  charCount.textContent = `${text.length} chars`;
}

codeEditor.addEventListener('input',  () => { updateLines(); updateCursor(); });
codeEditor.addEventListener('scroll', syncLinesScroll);
codeEditor.addEventListener('click',  updateCursor);
codeEditor.addEventListener('keyup',  updateCursor);
codeEditor.addEventListener('keydown', e => {
  if (e.key === 'Tab') {
    e.preventDefault();
    const s = codeEditor.selectionStart, ee = codeEditor.selectionEnd;
    codeEditor.value = codeEditor.value.slice(0, s) + '    ' + codeEditor.value.slice(ee);
    codeEditor.selectionStart = codeEditor.selectionEnd = s + 4;
    updateLines();
  }
});

// ── RESIZER ───────────────────────────────────────────────────
(function () {
  const handle = $('resizer'), left = $('paneEditor');
  let drag = false, startX = 0, startW = 0;
  handle.addEventListener('mousedown', e => {
    drag = true; startX = e.clientX; startW = left.offsetWidth;
    handle.classList.add('drag');
    document.body.style.cursor = 'col-resize';
    document.body.style.userSelect = 'none';
  });
  document.addEventListener('mousemove', e => {
    if (!drag) return;
    left.style.width = Math.max(240, Math.min(startW + e.clientX - startX, window.innerWidth - 300)) + 'px';
  });
  document.addEventListener('mouseup', () => {
    if (!drag) return;
    drag = false; handle.classList.remove('drag');
    document.body.style.cursor = '';
    document.body.style.userSelect = '';
  });
})();

// ── HEALTH CHECK ─────────────────────────────────────────────
(async function () {
  try {
    const data = await (await fetch('/api/health')).json();
    const dot = $('healthDot');
    dot.className = `health-dot ${data.ok && data.exists ? 'ok' : 'fail'}`;
    dot.title = data.exists ? `Compilador listo: ${data.compiler}` : `No encontrado: ${data.compiler}`;
  } catch {
    $('healthDot').className = 'health-dot fail';
    $('healthDot').title = 'Servidor no disponible';
  }
})();

// ── INIT ─────────────────────────────────────────────────────
codeEditor.value = SAMPLE;
updateLines();
updateCursor();
showTab('run'); // start on the Run tab