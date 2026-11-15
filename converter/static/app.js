const $ = (id) => document.getElementById(id);
const state = {
  browsers: { source: { path: null, parent: null }, card: { path: null, parent: null } },
  jobId: null,
  deployJobId: null,
  inspection: null,
  cardValid: false,
  config: null,
};

function showStep(name) {
  document.querySelectorAll('.step').forEach((button) => button.classList.toggle('active', button.dataset.step === name));
  document.querySelectorAll('.panel').forEach((panel) => panel.classList.toggle('hidden', panel.id !== name));
  document.querySelectorAll('.panel').forEach((panel) => panel.classList.toggle('active-panel', panel.id === name));
}

async function getJson(url) {
  const response = await fetch(url);
  const payload = await response.json();
  if (!response.ok || payload.error) throw new Error(payload.error || 'Request failed.');
  return payload;
}

async function postJson(url, body) {
  const response = await fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const payload = await response.json();
  if (!response.ok || payload.error) throw new Error(payload.error || 'Request failed.');
  return payload;
}

function showMessage(message, good = false) {
  $('validation').textContent = message;
  $('validation').className = `validation ${good ? 'good' : 'bad'}`;
}

function populateSelect(select, items, emptyLabel) {
  select.innerHTML = '';
  const empty = document.createElement('option');
  empty.value = '';
  empty.textContent = emptyLabel;
  select.appendChild(empty);
  items.forEach((item) => {
    const option = document.createElement('option');
    option.value = item.path;
    option.textContent = item.label || item.relative || item.path;
    select.appendChild(option);
  });
}

function preview(path) {
  if (!path) {
    $('previewCard').classList.add('hidden');
    return;
  }
  $('previewImage').src = `/api/preview?path=${encodeURIComponent(path)}`;
  $('previewName').textContent = path;
  $('previewCard').classList.remove('hidden');
}

async function openBrowser(path, target = 'source') {
  try {
    const data = await getJson(`/api/browse?path=${encodeURIComponent(path)}`);
    state.browsers[target] = { path: data.path, parent: data.parent };
    const browser = $(target === 'card' ? 'cardBrowser' : 'browser');
    const browsePath = $(target === 'card' ? 'cardBrowsePath' : 'browsePath');
    const browserEntries = $(target === 'card' ? 'cardBrowserEntries' : 'browserEntries');
    browsePath.textContent = data.path;
    browserEntries.innerHTML = '';
    data.entries.forEach((entry) => {
      const row = document.createElement('div');
      row.className = 'browser-entry';
      const open = document.createElement('button');
      open.className = entry.directory ? 'entry-name directory' : 'entry-name';
      open.textContent = `${entry.directory ? '▸ ' : entry.image ? '▧ ' : '· '}${entry.name}`;
      if (entry.directory) open.onclick = () => openBrowser(entry.path, target);
      else if (entry.image && target === 'source') open.onclick = () => {
        $('backgroundSource').value = entry.path;
        preview(entry.path);
      };
      row.appendChild(open);
      if (entry.directory) {
        const use = document.createElement('button');
        use.className = 'button tiny';
        use.textContent = 'Use folder';
        use.onclick = () => {
          if (target === 'card') {
            $('cardRoot').value = entry.path;
            $('cardBrowser').classList.add('hidden');
            checkCard();
          } else {
            $('sourceRoot').value = entry.path;
            $('browser').classList.add('hidden');
            inspectSource();
          }
        };
        row.appendChild(use);
      }
      browserEntries.appendChild(row);
    });
    browser.classList.remove('hidden');
  } catch (error) {
    showMessage(error.message);
  }
}

async function inspectSource() {
  try {
    const path = $('sourceRoot').value.trim();
    if (!path) throw new Error('Choose an extracted source folder first.');
    const data = await getJson(`/api/inspect?path=${encodeURIComponent(path)}`);
    state.inspection = data;
    $('imageCount').textContent = data.image_count;
    $('folderCount').textContent = data.folders.length;
    populateSelect($('backgroundSource'), data.images.map((item) => ({ path: item.path, label: item.relative })), 'Choose after scanning…');
    populateSelect($('artRoot'), data.folders.map((item) => ({ path: item.path, label: `${item.path} (${item.count} images)` })), 'No per-system artwork');
    populateSelect($('iconRoot'), data.folders.map((item) => ({ path: item.path, label: `${item.path} (${item.count} images)` })), 'Use no icon pack');
    $('inspection').classList.remove('hidden');
    showMessage(`Scanned ${data.image_count} image${data.image_count === 1 ? '' : 's'} in ${data.path}.`, true);
    if (data.images[0]) {
      $('backgroundSource').value = data.images[0].path;
      preview(data.images[0].path);
    }
  } catch (error) {
    $('inspection').classList.add('hidden');
    showMessage(error.message);
  }
}

function validate() {
  const mode = $('mode').value;
  if (!$('sourceRoot').value.trim()) return 'Choose an extracted source folder.';
  if (!$('packName').value.trim()) return 'Enter a pack name.';
  if (mode !== 'icons' && !$('backgroundSource').value) return 'Choose a main background image.';
  if (mode !== 'backgrounds' && !$('iconRoot').value) return 'Choose an icon artwork folder.';
  if (!$('outputRoot').value.trim()) return 'Choose an output staging folder.';
  return '';
}

function updateOutputSpec() {
  const configured = state.config && state.config.devices.find((entry) => entry.id === $('device').value);
  const width = configured ? configured.width : (['R36SX', 'R36HD', 'GB350'].includes($('device').value) ? 640 : 854);
  const height = configured ? configured.height : 480;
  const iconWidth = state.config ? state.config.output.icon_max_width : 160;
  const iconHeight = state.config ? state.config.output.icon_max_height : 130;
  $('outputSpec').textContent = `Target output: ${width}×${height} RGB backgrounds · ≤${iconWidth}×${iconHeight} optimized icons`;
}

async function startConversion() {
  const problem = validate();
  if (problem) {
    showMessage(problem);
    showStep('source');
    return;
  }
  const payload = {
    source_root: $('sourceRoot').value.trim(),
    background_source: $('backgroundSource').value,
    system_root: $('artRoot').value,
    icon_root: $('iconRoot').value,
    pack_name: $('packName').value.trim(),
    device: $('device').value,
    mode: $('mode').value,
    output_root: $('outputRoot').value.trim(),
    source_url: $('sourceUrl').value.trim(),
    license_note: $('licenseNote').value.trim(),
    dim: Number($('dim').value),
    quality: Number($('quality').value),
    overwrite: $('overwrite').checked,
  };
  try {
    $('installStep').disabled = true;
    $('integrityStatus').textContent = 'Waiting for the conversion integrity check.';
    $('integrityStatus').className = 'integrity';
    const result = await postJson('/api/convert', payload);
    state.jobId = result.job_id;
    $('progressTitle').textContent = 'Conversion in progress';
    $('progressBar').style.width = '0%';
    $('progressCount').textContent = '0 / 0';
    $('progressPercent').textContent = '0%';
    $('log').textContent = 'Starting…';
    $('result').classList.add('hidden');
    $('cancelButton').classList.remove('hidden');
    showStep('progress');
    pollJob();
  } catch (error) {
    showMessage(error.message);
  }
}

async function pollJob() {
  if (!state.jobId) return;
  try {
    const job = await getJson(`/api/jobs/${state.jobId}`);
    const total = job.total || 0;
    const percent = total ? Math.min(100, Math.round((job.completed / total) * 100)) : 0;
    $('progressBar').style.width = `${percent}%`;
    $('progressCount').textContent = `${job.completed} / ${total}`;
    $('progressPercent').textContent = `${percent}%`;
    $('log').textContent = (job.log || []).join('\n') || 'Preparing conversion…';
    if (job.state === 'complete') {
      $('progressTitle').textContent = 'Conversion complete';
      $('cancelButton').classList.add('hidden');
      const integrity = job.integrity || {};
      if (job.output_root) $('outputRoot').value = job.output_root;
      $('result').textContent = `Integrity check passed: ${integrity.backgrounds || 0} backgrounds and ${integrity.icons || 0} icons verified. Packs staged under “${job.output_root || $('outputRoot').value}”.`;
      $('result').className = 'result good';
      $('installStep').disabled = false;
      $('openOutputButton').classList.remove('hidden');
      $('integrityStatus').textContent = 'Integrity check passed. SHA-256 manifests, dimensions, formats, channels, and file counts are valid.';
      $('integrityStatus').className = 'integrity good';
      $('deployLog').textContent = 'Choose a mounted SD card root to continue.';
      showStep('install');
      return;
    }
    if (job.state === 'error' || job.state === 'cancelled') {
      $('progressTitle').textContent = job.state === 'cancelled' ? 'Conversion cancelled' : 'Conversion needs attention';
      $('cancelButton').classList.add('hidden');
      const integrity = job.integrity || {};
      const message = integrity.state === 'failed' ? `Integrity check failed: ${integrity.message}` : (job.error || 'The conversion did not complete.');
      $('result').textContent = message;
      $('result').className = 'result bad';
      $('integrityStatus').textContent = message;
      $('integrityStatus').className = 'integrity bad';
      return;
    }
    setTimeout(pollJob, 250);
  } catch (error) {
    $('log').textContent += `\n${error.message}`;
  }
}

async function checkCard() {
  try {
    const path = $('cardRoot').value.trim();
    if (!path) throw new Error('Choose the mounted SD card root first.');
    const data = await getJson(`/api/card-check?path=${encodeURIComponent(path)}`);
    state.cardValid = data.valid;
    $('deployButton').disabled = !data.valid;
    $('cardHint').textContent = data.valid
      ? `Mounted TreeFrogUI card found. Packs will copy to ${data.theme_target} and ${data.icon_target}.`
      : 'This folder does not contain the configured TreeFrogUI directory. Select the mounted card root, not its frogui folder.';
    $('cardHint').className = `hint ${data.valid ? 'good' : 'bad'}`;
  } catch (error) {
    state.cardValid = false;
    $('deployButton').disabled = true;
    $('cardHint').textContent = error.message;
    $('cardHint').className = 'hint bad';
  }
}

async function startDeploy() {
  if (!state.jobId || !state.cardValid) return;
  try {
    const result = await postJson('/api/deploy', {
      job_id: state.jobId,
      card_root: $('cardRoot').value.trim(),
      overwrite: $('deployOverwrite').checked,
    });
    state.deployJobId = result.job_id;
    $('deployButton').disabled = true;
    $('deployProgressBar').style.width = '0%';
    $('deployProgressCount').textContent = '0 / 0';
    $('deployProgressPercent').textContent = '0%';
    $('deployLog').textContent = 'Starting SD-card copy…';
    $('deployResult').classList.add('hidden');
    pollDeploy();
  } catch (error) {
    $('deployResult').textContent = error.message;
    $('deployResult').className = 'result bad';
  }
}

async function openOutputFolder() {
  if (!state.jobId) return;
  try {
    const result = await postJson('/api/open-output', { job_id: state.jobId });
    $('deployResult').textContent = `Opened ${result.path} in the operating system's file explorer.`;
    $('deployResult').className = 'result good';
  } catch (error) {
    $('deployResult').textContent = error.message;
    $('deployResult').className = 'result bad';
  }
}

async function pollDeploy() {
  if (!state.deployJobId) return;
  try {
    const job = await getJson(`/api/jobs/${state.deployJobId}`);
    const total = job.total || 0;
    const percent = total ? Math.min(100, Math.round((job.completed / total) * 100)) : 0;
    $('deployProgressBar').style.width = `${percent}%`;
    $('deployProgressCount').textContent = `${job.completed} / ${total}`;
    $('deployProgressPercent').textContent = `${percent}%`;
    $('deployLog').textContent = (job.log || []).join('\n') || 'Preparing SD-card copy…';
    if (job.card_root) $('cardRoot').value = job.card_root;
    if (job.state === 'complete') {
      $('deployResult').textContent = `Copied the generated pack files to ${job.card_root}. The SD card can now be safely unmounted and returned to the device.`;
      $('deployResult').className = 'result good';
      $('integrityStatus').textContent = 'SD-card integrity check passed. The staged files were verified before commit.';
      $('integrityStatus').className = 'integrity good';
      $('deployButton').disabled = false;
      return;
    }
    if (job.state === 'error' || job.state === 'cancelled') {
      const message = job.integrity && job.integrity.state === 'failed'
        ? `SD-card integrity check failed: ${job.integrity.message}`
        : (job.error || 'The SD-card copy did not complete.');
      $('deployResult').textContent = message;
      $('deployResult').className = 'result bad';
      $('integrityStatus').textContent = message;
      $('integrityStatus').className = 'integrity bad';
      $('deployButton').disabled = false;
      return;
    }
    setTimeout(pollDeploy, 250);
  } catch (error) {
    $('deployLog').textContent += `\n${error.message}`;
  }
}

async function recoverJob() {
  try {
    const response = await getJson('/api/current-job');
    const job = response.job;
    if (!job) return;
    if (job.kind === 'deploy') {
      state.deployJobId = job.id;
      state.jobId = job.source_job_id;
      $('installStep').disabled = false;
      $('openOutputButton').classList.remove('hidden');
      if (job.card_root) $('cardRoot').value = job.card_root;
      showStep('install');
      pollDeploy();
      return;
    }
    state.jobId = job.id;
    $('progressTitle').textContent = job.state === 'complete' ? 'Conversion complete' : 'Conversion in progress';
    $('log').textContent = (job.log || []).join('\n') || 'Reconnected to the existing job…';
    showStep('progress');
    pollJob();
  } catch (error) {
    // A fresh server has no current job; the normal source screen remains available.
  }
}

async function cancelJob() {
  if (state.jobId) await postJson(`/api/jobs/${state.jobId}/cancel`, {});
}

function openShutdownModal() {
  $('shutdownMessage').textContent = 'Any active conversion or SD-card copy will be cancelled before the local server exits. Choose Yes only if you want to stop it now.';
  $('shutdownYes').disabled = false;
  $('shutdownNo').disabled = false;
  $('shutdownModal').classList.remove('hidden');
}

function closeShutdownModal() {
  $('shutdownModal').classList.add('hidden');
}

async function shutdownServer() {
  $('shutdownYes').disabled = true;
  $('shutdownNo').disabled = true;
  $('shutdownMessage').textContent = 'Stopping active work and shutting down the local server…';
  try {
    const result = await postJson('/api/shutdown', { confirmation: 'Yes' });
    $('shutdownMessage').textContent = result.active_jobs
      ? `Cancelled ${result.active_jobs} active job${result.active_jobs === 1 ? '' : 's'}. The server has stopped; you can close this tab.`
      : 'The server has stopped. You can close this tab.';
    $('stopButton').textContent = 'Server stopped';
    $('stopButton').disabled = true;
  } catch (error) {
    $('shutdownMessage').textContent = error.message;
    $('shutdownYes').disabled = false;
    $('shutdownNo').disabled = false;
  }
}

async function checkDependencies() {
  try {
    const data = await getJson('/api/dependencies');
    const badge = $('dependencyBadge');
    badge.textContent = data.ready ? `ImageMagick ready · Python ${data.python}` : 'Install ImageMagick';
    badge.className = `badge ${data.ready ? 'ready' : 'warning'}`;
  } catch (error) {
    $('dependencyBadge').textContent = 'Dependency check failed';
    $('dependencyBadge').className = 'badge warning';
  }
}

async function loadConfig() {
  try {
    const config = await getJson('/api/config');
    state.config = config;
    const device = $('device');
    device.innerHTML = '';
    config.devices.forEach((entry) => {
      const option = document.createElement('option');
      option.value = entry.id;
      option.textContent = `${entry.label} — ${entry.width}×${entry.height}`;
      if (entry.id === config.default_device) option.selected = true;
      device.appendChild(option);
    });
    const frogui = config.treefrogui.frogui_directory;
    $('cardHint').innerHTML = `Choose the mount root containing <code>${frogui}</code>, not the <code>${frogui}</code> folder itself.`;
    updateOutputSpec();
  } catch (error) {
    // The HTML device list remains usable if the optional config request fails.
  }
}

document.querySelectorAll('.step').forEach((button) => button.addEventListener('click', () => showStep(button.dataset.step)));
$('browseButton').addEventListener('click', () => openBrowser($('sourceRoot').value.trim() || '/', 'source'));
$('parentButton').addEventListener('click', () => state.browsers.source.parent && openBrowser(state.browsers.source.parent, 'source'));
$('cardBrowseButton').addEventListener('click', () => openBrowser($('cardRoot').value.trim() || '/', 'card'));
$('cardParentButton').addEventListener('click', () => state.browsers.card.parent && openBrowser(state.browsers.card.parent, 'card'));
$('inspectButton').addEventListener('click', inspectSource);
$('backgroundSource').addEventListener('change', (event) => preview(event.target.value));
$('convertButton').addEventListener('click', startConversion);
$('cancelButton').addEventListener('click', cancelJob);
$('checkCardButton').addEventListener('click', checkCard);
$('deployButton').addEventListener('click', startDeploy);
$('openOutputButton').addEventListener('click', openOutputFolder);
$('stopButton').addEventListener('click', openShutdownModal);
$('shutdownNo').addEventListener('click', closeShutdownModal);
$('shutdownYes').addEventListener('click', shutdownServer);
$('dim').addEventListener('input', (event) => $('dimValue').textContent = `${event.target.value}%`);
$('quality').addEventListener('input', (event) => $('qualityValue').textContent = event.target.value);
$('device').addEventListener('change', updateOutputSpec);
$('mode').addEventListener('change', () => {
  const mode = $('mode').value;
  $('backgroundSource').disabled = mode === 'icons';
  $('iconRoot').disabled = mode === 'backgrounds';
});
checkDependencies();
loadConfig();
updateOutputSpec();
recoverJob();
