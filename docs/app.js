/* ============ Data Model ============ */

class Portfolio {
  constructor() {
    this.assets = [];
  }

  totalCents() {
    return this.assets.reduce((s, a) => s + a.amount_cents, 0);
  }

  findAsset(name) {
    return this.assets.find(a => a.name === name);
  }

  upsertAsset(name, amountCents) {
    name = name.trim();
    if (!name || amountCents < 0 || amountCents > 9000000000000000) return false;
    const existing = this.findAsset(name);
    if (existing) {
      if (this.earmarkedTotalCents(name) > amountCents) return false;
      existing.amount_cents = amountCents;
    } else {
      this.assets.push({ name, amount_cents: amountCents, earmarks: [] });
    }
    this.assets.sort((a, b) => a.name.localeCompare(b.name));
    return true;
  }

  removeAsset(name) {
    this.assets = this.assets.filter(a => a.name !== name);
  }

  earmarkedTotalCents(assetName) {
    const asset = this.findAsset(assetName);
    if (!asset) return 0;
    return asset.earmarks.reduce((s, e) => s + e.amount_cents, 0);
  }

  upsertEarmark(assetName, earmarkName, amountCents) {
    earmarkName = earmarkName.trim();
    if (!earmarkName || amountCents < 0 || amountCents > 9000000000000000) return false;
    const asset = this.findAsset(assetName);
    if (!asset) return false;
    const existing = asset.earmarks.find(e => e.name === earmarkName);
    const current = existing ? existing.amount_cents : 0;
    const projected = this.earmarkedTotalCents(assetName) - current + amountCents;
    if (projected > asset.amount_cents) return false;
    if (existing) {
      existing.amount_cents = amountCents;
    } else {
      asset.earmarks.push({ name: earmarkName, amount_cents: amountCents });
    }
    return true;
  }

  removeEarmark(assetName, earmarkName) {
    const asset = this.findAsset(assetName);
    if (!asset) return;
    asset.earmarks = asset.earmarks.filter(e => e.name !== earmarkName);
  }

  distribution() {
    const total = this.totalCents();
    return this.assets.map(a => ({
      label: a.name,
      amount_cents: a.amount_cents,
      percentage: total > 0 ? (a.amount_cents * 100.0 / total) : 0.0
    }));
  }
}

/* ============ File Format (Pipe-Delimited, C++ compatible) ============ */

function parsePortfolio(text) {
  const p = new Portfolio();
  const lines = text.split('\n');
  for (const line of lines) {
    if (!line.trim()) continue;
    const fields = splitEscaped(line.trim());
    try {
      if (fields.length === 3 && fields[0] === 'ASSET') {
        p.upsertAsset(fields[1], parseInt(fields[2], 10));
      } else if (fields.length === 4 && fields[0] === 'EARMARK') {
        p.upsertEarmark(fields[1], fields[2], parseInt(fields[3], 10));
      }
    } catch (e) { /* skip malformed lines */ }
  }
  return p;
}

function serializePortfolio(p) {
  let out = '';
  for (const asset of p.assets) {
    out += 'ASSET|' + escapeField(asset.name) + '|' + asset.amount_cents + '\n';
    for (const earmark of asset.earmarks) {
      out += 'EARMARK|' + escapeField(asset.name) + '|' + escapeField(earmark.name) + '|' + earmark.amount_cents + '\n';
    }
  }
  return out;
}

function escapeField(value) {
  return value.replace(/\\/g, '\\\\').replace(/\|/g, '\\|');
}

function splitEscaped(line) {
  const fields = [];
  let cur = '';
  let esc = false;
  for (const ch of line) {
    if (esc) { cur += ch; esc = false; continue; }
    if (ch === '\\') { esc = true; continue; }
    if (ch === '|') { fields.push(cur); cur = ''; continue; }
    cur += ch;
  }
  fields.push(cur);
  return fields;
}

/* ============ Format Helpers ============ */

const PIE_COLORS = [
  '#2563eb', '#dc2626', '#16a34a', '#ca8a04', '#7c3aed',
  '#0891b2', '#db2777', '#4b5563', '#ea580c', '#0f766e'
];

function formatMoney(cents) {
  if (cents == null) return '₹0.00';
  const neg = cents < 0;
  if (neg) cents = -cents;
  const rupees = Math.floor(cents / 100);
  const paise = cents % 100;
  return (neg ? '-₹' : '₹') + rupees.toLocaleString('en-IN') + '.' + String(paise).padStart(2, '0');
}

function parseMoneyInput(str) {
  const cleaned = str.trim().replace(/,/g, '');
  const m = cleaned.match(/^(\d+)(?:\.(\d*))?$/);
  if (!m) return null;
  let rupees = parseInt(m[1], 10);
  if (rupees > 90000000000000) return null;
  let paise = 0;
  if (m[2] !== undefined) {
    const dec = m[2].slice(0, 2);
    paise = parseInt(dec.padEnd(2, '0'), 10);
  }
  const total = rupees * 100 + paise;
  if (total > 9000000000000000) return null;
  return total;
}

/* ============ State ============ */

let portfolio = new Portfolio();
let currentUser = null;
let driveFileId = null;
let driveFileModifiedTime = null;
let lastSyncTime = null;
let isOnlineMode = false;
let isSaving = false;

const LS_KEY = 'finance_portfolio';

/* ============ Google Drive OAuth ============ */

let tokenClient = null;
let accessToken = null;

function initGoogleApi() {
  if (typeof google === 'undefined' || !google.accounts) {
    setTimeout(initGoogleApi, 500);
    return;
  }
  tokenClient = google.accounts.oauth2.initTokenClient({
    client_id: '26076813279-kq5gdq1dnq2trsleg1s7f7g27delvrk5.apps.googleusercontent.com',
    scope: 'https://www.googleapis.com/auth/drive.file',
    callback: tokenResponse => {
      if (tokenResponse.access_token) {
        accessToken = tokenResponse.access_token;
        currentUser = 'user';
        isOnlineMode = true;
        document.getElementById('auth-screen').style.display = 'none';
        document.getElementById('main-app').style.display = 'flex';
        updateDriveUI();
        showToast('Connected to Google Drive');
        syncFromDrive().then(() => renderAll());
      } else {
        showToast('Sign-in cancelled', true);
      }
    }
  });
  updateDriveUI();
}

function handleSignIn() {
  if (!tokenClient) {
    showToast('Google API not loaded. Check your internet.', true);
    return;
  }
  tokenClient.requestAccessToken();
}

function handleSignOut() {
  accessToken = null;
  currentUser = null;
  driveFileId = null;
  driveFileModifiedTime = null;
  isOnlineMode = false;
  updateDriveUI();
  saveLocal();
  renderAll();
  showToast('Signed out');
}

function startOffline() {
  document.getElementById('auth-screen').style.display = 'none';
  document.getElementById('main-app').style.display = 'flex';
  loadLocal();
  renderAll();
}

function onDriveAction() {
  if (isOnlineMode) handleSignOut();
  else handleSignIn();
}

/* ============ Google Drive API ============ */

async function driveFetch(url, options = {}) {
  if (!accessToken) throw new Error('Not authenticated');
  options.headers = options.headers || {};
  options.headers['Authorization'] = 'Bearer ' + accessToken;
  const res = await fetch(url, options);
  if (res.status === 401) {
    showToast('Session expired. Please sign in again.', true);
    handleSignOut();
    throw new Error('Unauthorized');
  }
  return res;
}

async function findDriveFile() {
  const q = encodeURIComponent("name='portfolio.txt' and trashed=false");
  const res = await driveFetch(
    'https://www.googleapis.com/drive/v3/files?q=' + q + '&fields=files(id,name,modifiedTime)'
  );
  const data = await res.json();
  if (data.files && data.files.length > 0) {
    return data.files[0];
  }
  return null;
}

async function readDriveFile(fileId) {
  const res = await driveFetch(
    'https://www.googleapis.com/drive/v3/files/' + fileId + '?alt=media'
  );
  return await res.text();
}

async function createDriveFile(content) {
  const boundary = 'boundary_' + Date.now();
  const body = [
    '--' + boundary,
    'Content-Type: application/json; charset=UTF-8',
    '',
    JSON.stringify({ name: 'portfolio.txt' }),
    '--' + boundary,
    'Content-Type: text/plain; charset=UTF-8',
    '',
    content,
    '--' + boundary + '--'
  ].join('\r\n');

  const res = await driveFetch(
    'https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart',
    {
      method: 'POST',
      headers: { 'Content-Type': 'multipart/related; boundary=' + boundary },
      body: body
    }
  );
  const data = await res.json();
  driveFileId = data.id;
  driveFileModifiedTime = new Date().toISOString();
  return data;
}

async function updateDriveFile(fileId, content) {
  const boundary = 'boundary_' + Date.now();
  const body = [
    '--' + boundary,
    'Content-Type: application/json; charset=UTF-8',
    '',
    JSON.stringify({}),
    '--' + boundary,
    'Content-Type: text/plain; charset=UTF-8',
    '',
    content,
    '--' + boundary + '--'
  ].join('\r\n');

  const res = await driveFetch(
    'https://www.googleapis.com/upload/drive/v3/files/' + fileId + '?uploadType=multipart',
    {
      method: 'PATCH',
      headers: { 'Content-Type': 'multipart/related; boundary=' + boundary },
      body: body
    }
  );
  const data = await res.json();
  driveFileModifiedTime = new Date().toISOString();
  return data;
}

async function getDriveFileMeta(fileId) {
  const res = await driveFetch(
    'https://www.googleapis.com/drive/v3/files/' + fileId + '?fields=id,name,modifiedTime'
  );
  return await res.json();
}

async function syncFromDrive() {
  if (!isOnlineMode) return false;
  try {
    setSyncStatus('syncing');
    let meta = await findDriveFile();
    if (!meta) {
      setSyncStatus('connected');
      return false;
    }
    driveFileId = meta.id;
    driveFileModifiedTime = meta.modifiedTime;
    const content = await readDriveFile(meta.id);
    if (content && content.trim()) {
      portfolio = parsePortfolio(content);
      saveLocal();
      lastSyncTime = new Date().toISOString();
      setSyncStatus('connected');
      return true;
    }
    setSyncStatus('connected');
    return false;
  } catch (e) {
    console.error('Sync from Drive failed:', e);
    setSyncStatus('error');
    return false;
  }
}

async function syncToDrive() {
  if (!isOnlineMode) return false;
  if (isSaving) return false;
  isSaving = true;
  try {
    setSyncStatus('syncing');
    if (!driveFileId) {
      const found = await findDriveFile();
      if (found) {
        driveFileId = found.id;
        driveFileModifiedTime = found.modifiedTime;
      }
    }
    const content = serializePortfolio(portfolio);
    if (driveFileId) {
      await updateDriveFile(driveFileId, content);
    } else {
      await createDriveFile(content);
    }
    lastSyncTime = new Date().toISOString();
    setSyncStatus('connected');
    isSaving = false;
    return true;
  } catch (e) {
    console.error('Sync to Drive failed:', e);
    setSyncStatus('error');
    isSaving = false;
    return false;
  }
}

/* ============ Local Storage ============ */

function saveLocal() {
  try {
    localStorage.setItem(LS_KEY, serializePortfolio(portfolio));
  } catch (e) { /* storage full - ignore */ }
}

function loadLocal() {
  try {
    const data = localStorage.getItem(LS_KEY);
    if (data) portfolio = parsePortfolio(data);
  } catch (e) { /* ignore */ }
}

/* ============ UI State Helpers ============ */

function setSyncStatus(status) {
  const el = document.getElementById('sync-status');
  if (!el) return;
  el.className = 'sync-status';
  if (status === 'syncing') {
    el.textContent = '\u21BB';
    el.classList.add('syncing');
  } else if (status === 'connected') {
    el.textContent = '\u2713';
    el.style.color = '#22c55e';
  } else if (status === 'error') {
    el.textContent = '\u2717';
    el.style.color = '#ef4444';
  } else {
    el.textContent = '\u25CB';
    el.style.color = '#94a3b8';
  }
}

function updateDriveUI() {
  const status = document.getElementById('drive-status');
  const info = document.getElementById('drive-info');
  const btn = document.getElementById('drive-action-btn');
  if (!status || !info || !btn) return;
  if (isOnlineMode) {
    status.className = 'drive-status connected';
    info.innerHTML = '<p style="color:#22c55e;font-weight:600">Connected to Google Drive</p>'
      + (lastSyncTime ? '<p style="font-size:12px;color:#64748b">Last sync: ' + new Date(lastSyncTime).toLocaleString() + '</p>' : '');
    btn.textContent = 'Disconnect';
    btn.className = 'btn btn-outline';
  } else {
    status.className = 'drive-status disconnected';
    info.innerHTML = '<p>Not connected. Data is saved locally only.</p>';
    btn.textContent = 'Sign in with Google';
    btn.className = 'btn btn-primary';
  }
}

function showToast(msg, isError = false) {
  const el = document.getElementById('toast');
  if (!el) return;
  el.textContent = msg;
  el.style.background = isError ? '#dc2626' : '#0f172a';
  el.classList.add('show');
  clearTimeout(el._timer);
  el._timer = setTimeout(() => el.classList.remove('show'), 3000);
}

/* ============ Tab Navigation ============ */

function showTab(name) {
  document.querySelectorAll('.tab-pane').forEach(el => el.classList.remove('active'));
  document.getElementById('tab-' + name).classList.add('active');
  document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
  document.querySelector('.nav-item[data-tab="' + name + '"]').classList.add('active');
  if (name === 'dashboard') renderDashboard();
  else if (name === 'categories') renderCategories();
  else if (name === 'earmarks') renderEarmarks();
  else if (name === 'settings') updateDriveUI();
}

/* ============ Notify & Persist ============ */

function notifyChange() {
  saveLocal();
  if (isOnlineMode) syncToDrive();
  renderAll();
}

function renderAll() {
  renderDashboard();
  renderCategories();
  renderEarmarks();
  updateDriveUI();
}

/* ============ Pie Chart ============ */

function renderPieChart() {
  const canvas = document.getElementById('pie-chart');
  const legend = document.getElementById('pie-legend');
  if (!canvas || !legend) return;
  const ctx = canvas.getContext('2d');
  const rect = canvas.parentElement.getBoundingClientRect();
  const size = Math.min(rect.width - 32, 300, window.innerWidth - 64);
  const dpr = window.devicePixelRatio || 1;
  canvas.width = size * dpr;
  canvas.height = size * dpr;
  canvas.style.width = size + 'px';
  canvas.style.height = size + 'px';
  ctx.scale(dpr, dpr);

  ctx.clearRect(0, 0, size, size);

  const slices = portfolio.distribution();
  const total = portfolio.totalCents();

  legend.innerHTML = '';
  if (slices.length === 0) {
    ctx.fillStyle = '#e2e8f0';
    ctx.beginPath();
    ctx.arc(size / 2, size / 2, size * 0.4, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#64748b';
    ctx.font = '14px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('No data yet', size / 2, size / 2 + 5);
    return;
  }

  const cx = size / 2, cy = size / 2;
  const outerR = size * 0.42;
  const innerR = outerR * 0.55;

  let startAngle = -Math.PI / 2;
  for (let i = 0; i < slices.length; i++) {
    const sweep = 2 * Math.PI * slices[i].amount_cents / total;
    const endAngle = startAngle + sweep;

    ctx.beginPath();
    ctx.arc(cx, cy, outerR, startAngle, endAngle);
    ctx.arc(cx, cy, innerR, endAngle, startAngle, true);
    ctx.closePath();
    ctx.fillStyle = PIE_COLORS[i % PIE_COLORS.length];
    ctx.fill();

    startAngle = endAngle;
  }

  // Legend
  for (let i = 0; i < slices.length; i++) {
    const item = document.createElement('div');
    item.className = 'pie-legend-item';
    item.innerHTML = '<span class="pie-legend-dot" style="background:' + PIE_COLORS[i % PIE_COLORS.length] + '"></span>'
      + slices[i].label + ' ' + formatMoney(slices[i].amount_cents)
      + ' (' + slices[i].percentage.toFixed(1) + '%)';
    legend.appendChild(item);
  }

  function getSliceAtPos(clientX, clientY) {
    const r = canvas.getBoundingClientRect();
    const mx = (clientX - r.left) * size / r.width;
    const my = (clientY - r.top) * size / r.height;
    const dx = mx - cx, dy = my - cy;
    const dist = Math.sqrt(dx * dx + dy * dy);
    if (dist > outerR || dist < innerR) return -1;
    let angle = Math.atan2(dy, dx) + Math.PI / 2;
    if (angle < 0) angle += 2 * Math.PI;
    const pct = angle / (2 * Math.PI);
    let accum = 0;
    for (let i = 0; i < slices.length; i++) {
      accum += slices[i].amount_cents / total;
      if (pct <= accum) return i;
    }
    return -1;
  }

  function updateTooltip(clientX, clientY) {
    const idx = getSliceAtPos(clientX, clientY);
    if (idx >= 0) {
      const s = slices[idx];
      canvas.title = s.label + '\n' + formatMoney(s.amount_cents) + ' (' + s.percentage.toFixed(1) + '%)';
    } else {
      canvas.title = '';
    }
  }

  canvas.onmousemove = function (e) { updateTooltip(e.clientX, e.clientY); };
  canvas.onmouseleave = function () { canvas.title = ''; };
  canvas.ontouchstart = function (e) {
    const t = e.touches[0];
    if (t) updateTooltip(t.clientX, t.clientY);
  };
  canvas.ontouchmove = function (e) {
    const t = e.touches[0];
    if (t) updateTooltip(t.clientX, t.clientY);
  };
  canvas.ontouchend = function () { canvas.title = ''; };
}

/* ============ Dashboard Tab ============ */

function renderDashboard() {
  document.getElementById('total-amount').textContent = formatMoney(portfolio.totalCents());
  renderPieChart();
  renderDashboardList();
  renderDashboardEarmarks();
}

function renderDashboardList() {
  const el = document.getElementById('dashboard-list');
  const slices = portfolio.distribution();
  if (slices.length === 0) {
    el.innerHTML = '<div class="empty-state">No categories yet. Add one in the Categories tab.</div>';
    return;
  }
  el.innerHTML = slices.map((s, i) =>
    '<div class="dash-cat">'
      + '<div class="dash-cat-left">'
        + '<span class="dash-cat-dot" style="background:' + PIE_COLORS[i % PIE_COLORS.length] + '"></span>'
        + '<span class="dash-cat-name">' + escHtml(s.label) + '</span>'
      + '</div>'
      + '<div class="dash-cat-right">'
        + '<div class="dash-cat-amount">' + formatMoney(s.amount_cents) + '</div>'
        + '<div class="dash-cat-pct">' + s.percentage.toFixed(1) + '%</div>'
      + '</div>'
    + '</div>'
  ).join('');
}

function renderDashboardEarmarks() {
  const el = document.getElementById('dashboard-earmarks');
  let has = false;
  let html = '';
  for (const asset of portfolio.assets) {
    for (const earmark of asset.earmarks) {
      has = true;
      html += '<div class="dash-cat">'
        + '<div class="dash-cat-left">'
          + '<span class="dash-cat-name">' + escHtml(asset.name) + ' → ' + escHtml(earmark.name) + '</span>'
        + '</div>'
        + '<div class="dash-cat-right">'
          + '<div class="dash-cat-amount">' + formatMoney(earmark.amount_cents) + '</div>'
        + '</div>'
      + '</div>';
    }
  }
  el.innerHTML = has ? html : '<div class="empty-state">No earmarks yet.</div>';
}

/* ============ Categories Tab ============ */

function onSaveCategory(e) {
  e.preventDefault();
  const name = document.getElementById('cat-name').value.trim();
  const amountStr = document.getElementById('cat-amount').value.trim();
  const editName = document.getElementById('cat-edit-name').value;
  const mode = document.querySelector('input[name="cat-mode"]:checked').value;
  const amount = parseMoneyInput(amountStr);
  if (!name || !amount) {
    showToast('Enter a valid category name and amount.', true);
    return false;
  }

  if (mode === 'add' && portfolio.findAsset(name)) {
    const existing = portfolio.findAsset(name);
    if (!portfolio.upsertAsset(name, existing.amount_cents + amount)) {
      showToast('Cannot add: the total would exceed earmarks.', true);
      return false;
    }
  } else {
    if (!portfolio.upsertAsset(name, amount)) {
      showToast('Cannot save: check the amount and existing earmarks.', true);
      return false;
    }
  }

  document.getElementById('category-form').reset();
  document.getElementById('cat-edit-name').value = '';
  document.getElementById('cat-cancel-btn').style.display = 'none';
  document.getElementById('cat-form-title').textContent = 'Add Category';
  document.getElementById('cat-save-btn').textContent = 'Save';
  document.querySelector('input[name="cat-mode"][value="replace"]').checked = true;
  notifyChange();
  showToast('Category saved');
  return false;
}

function editCategory(name) {
  const asset = portfolio.findAsset(name);
  if (!asset) return;
  document.getElementById('cat-name').value = asset.name;
  document.getElementById('cat-amount').value = (asset.amount_cents / 100).toFixed(2);
  document.getElementById('cat-edit-name').value = asset.name;
  document.getElementById('cat-cancel-btn').style.display = 'inline-block';
  document.getElementById('cat-form-title').textContent = 'Edit Category';
  document.getElementById('cat-save-btn').textContent = 'Update';
  document.querySelector('input[name="cat-mode"][value="replace"]').checked = true;
  document.getElementById('tab-categories').scrollTop = 0;
  document.getElementById('cat-name').focus();
}

function cancelCategoryEdit() {
  document.getElementById('category-form').reset();
  document.getElementById('cat-edit-name').value = '';
  document.getElementById('cat-cancel-btn').style.display = 'none';
  document.getElementById('cat-form-title').textContent = 'Add Category';
  document.getElementById('cat-save-btn').textContent = 'Save';
  document.querySelector('input[name="cat-mode"][value="replace"]').checked = true;
}

function deleteCategory(name) {
  if (!confirm('Delete "' + name + '" and all its earmarks?')) return;
  portfolio.removeAsset(name);
  notifyChange();
  showToast('Category deleted');
}

function renderCategories() {
  const el = document.getElementById('categories-list');
  if (portfolio.assets.length === 0) {
    el.innerHTML = '<div class="empty-state">No categories yet.</div>';
    return;
  }
  el.innerHTML = portfolio.assets.map(a =>
    '<div class="list-item">'
      + '<div class="list-item-left">'
        + '<div class="list-item-name">' + escHtml(a.name) + '</div>'
        + '<div class="list-item-sub">' + a.earmarks.length + ' earmarks</div>'
      + '</div>'
      + '<div class="list-item-right">'
        + '<div class="list-item-amount">' + formatMoney(a.amount_cents) + '</div>'
      + '</div>'
      + '<div class="item-actions">'
        + '<button class="item-btn edit-btn" onclick="editCategory(\'' + jsEsc(a.name) + '\')" title="Edit">&#9998;</button>'
        + '<button class="item-btn del-btn" onclick="deleteCategory(\'' + jsEsc(a.name) + '\')" title="Delete">&#10005;</button>'
      + '</div>'
    + '</div>'
  ).join('');
}

/* ============ Earmarks Tab ============ */

function onEarmarkAssetChange() {
  renderEarmarks();
}

function onSaveEarmark(e) {
  e.preventDefault();
  const assetName = document.getElementById('earmark-asset-select').value;
  if (!assetName) { showToast('Select an investment category first.', true); return false; }
  const purpose = document.getElementById('earmark-purpose').value.trim();
  const amountStr = document.getElementById('earmark-amount').value.trim();
  const editName = document.getElementById('earmark-edit-name').value;
  const amount = parseMoneyInput(amountStr);
  if (!purpose || !amount) {
    showToast('Enter a valid purpose and amount.', true);
    return false;
  }

  if (!portfolio.upsertEarmark(assetName, purpose, amount)) {
    showToast('Cannot save: exceeds available amount for this category.', true);
    return false;
  }

  document.getElementById('earmark-form').reset();
  document.getElementById('earmark-edit-name').value = '';
  document.getElementById('earmark-cancel-btn').style.display = 'none';
  document.getElementById('earmark-form-title').textContent = 'Add Earmark';
  document.getElementById('earmark-save-btn').textContent = 'Save';
  document.getElementById('earmark-asset-select').value = assetName;
  notifyChange();
  showToast('Earmark saved');
  return false;
}

function editEarmark(assetName, earmarkName) {
  const asset = portfolio.findAsset(assetName);
  if (!asset) return;
  const earmark = asset.earmarks.find(e => e.name === earmarkName);
  if (!earmark) return;
  document.getElementById('earmark-asset-select').value = assetName;
  document.getElementById('earmark-purpose').value = earmark.name;
  document.getElementById('earmark-amount').value = (earmark.amount_cents / 100).toFixed(2);
  document.getElementById('earmark-edit-name').value = earmark.name;
  document.getElementById('earmark-cancel-btn').style.display = 'inline-block';
  document.getElementById('earmark-form-title').textContent = 'Edit Earmark';
  document.getElementById('earmark-save-btn').textContent = 'Update';
  document.getElementById('tab-earmarks').scrollTop = 0;
  document.getElementById('earmark-purpose').focus();
}

function cancelEarmarkEdit() {
  document.getElementById('earmark-form').reset();
  document.getElementById('earmark-edit-name').value = '';
  document.getElementById('earmark-cancel-btn').style.display = 'none';
  document.getElementById('earmark-form-title').textContent = 'Add Earmark';
  document.getElementById('earmark-save-btn').textContent = 'Save';
}

function deleteEarmark(assetName, earmarkName) {
  if (!confirm('Delete earmark "' + earmarkName + '"?')) return;
  portfolio.removeEarmark(assetName, earmarkName);
  notifyChange();
  showToast('Earmark deleted');
}

function renderEarmarks() {
  const select = document.getElementById('earmark-asset-select');
  const currentVal = select.value;
  select.innerHTML = '<option value="">-- Select --</option>'
    + portfolio.assets.map(a => '<option value="' + jsEsc(a.name) + '">' + escHtml(a.name) + ' (' + formatMoney(a.amount_cents) + ')</option>').join('');
  select.value = currentVal;

  const assetName = select.value;
  const el = document.getElementById('earmarks-list');
  if (!assetName) {
    el.innerHTML = '<div class="empty-state">Select an investment category above.</div>';
    return;
  }
  const asset = portfolio.findAsset(assetName);
  if (!asset || asset.earmarks.length === 0) {
    el.innerHTML = '<div class="empty-state">No earmarks for this category.</div>';
    return;
  }

  // Bar chart
  const barHtml = renderEarmarkBar(asset);

  // List
  const listHtml = asset.earmarks.map(e =>
    '<div class="earmark-item">'
      + '<div class="list-item-left">'
        + '<div class="list-item-name">' + escHtml(e.name) + '</div>'
        + '<div class="list-item-sub">' + formatMoney(e.amount_cents) + '</div>'
      + '</div>'
      + '<div class="item-actions">'
        + '<button class="item-btn edit-btn" onclick="editEarmark(\'' + jsEsc(assetName) + '\',\'' + jsEsc(e.name) + '\')" title="Edit">&#9998;</button>'
        + '<button class="item-btn del-btn" onclick="deleteEarmark(\'' + jsEsc(assetName) + '\',\'' + jsEsc(e.name) + '\')" title="Delete">&#10005;</button>'
      + '</div>'
    + '</div>'
  ).join('');

  el.innerHTML = barHtml + listHtml;
}

function renderEarmarkBar(asset) {
  if (asset.earmarks.length === 0) return '';
  const total = asset.amount_cents;
  let html = '<div class="earmark-bar-container">'
    + '<div class="earmark-bar">';
  for (let i = 0; i < asset.earmarks.length; i++) {
    const pct = total > 0 ? (asset.earmarks[i].amount_cents / total * 100) : 0;
    const width = Math.max(2, pct);
    html += '<div class="earmark-bar-seg" style="width:' + width + '%;background:' + PIE_COLORS[i % PIE_COLORS.length] + '">'
      + (pct > 15 ? asset.earmarks[i].name : '')
      + '</div>';
  }
  html += '</div>'
    + '<div class="earmark-legend">';
  for (let i = 0; i < asset.earmarks.length; i++) {
    const pct = total > 0 ? (asset.earmarks[i].amount_cents / total * 100) : 0;
    html += '<span class="earmark-legend-item">'
      + '<span class="earmark-legend-dot" style="background:' + PIE_COLORS[i % PIE_COLORS.length] + '"></span>'
      + escHtml(asset.earmarks[i].name) + ' ' + formatMoney(asset.earmarks[i].amount_cents)
      + ' (' + pct.toFixed(1) + '%)'
      + '</span>';
  }
  html += '</div></div>';
  return html;
}

/* ============ Settings Tab ============ */

function exportData() {
  const text = serializePortfolio(portfolio);
  const blob = new Blob([text], { type: 'text/plain' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'portfolio.txt';
  a.click();
  URL.revokeObjectURL(url);
  showToast('Exported portfolio.txt');
}

function importData(event) {
  const file = event.target.files[0];
  if (!file) return;
  const reader = new FileReader();
  reader.onload = function (e) {
    portfolio = parsePortfolio(e.target.result);
    notifyChange();
    showToast('Imported ' + file.name);
  };
  reader.readAsText(file);
  event.target.value = '';
}

function clearAll() {
  if (!confirm('Delete ALL data? This cannot be undone.')) return;
  portfolio = new Portfolio();
  notifyChange();
  showToast('All data cleared');
}

/* ============ Utility ============ */

function escHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

function jsEsc(str) {
  return str.replace(/'/g, "\\'").replace(/"/g, "&quot;");
}

/* ============ Init ============ */

function init() {
  // Register service worker
  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('service-worker.js');
  }

  // Load Google Identity Services
  const gs = document.createElement('script');
  gs.src = 'https://accounts.google.com/gsi/client';
  gs.onload = initGoogleApi;
  document.head.appendChild(gs);

  // Try to load from local
  loadLocal();

  // Show auth screen
  document.getElementById('auth-screen').style.display = 'flex';
  document.getElementById('main-app').style.display = 'none';
}

document.addEventListener('DOMContentLoaded', init);
