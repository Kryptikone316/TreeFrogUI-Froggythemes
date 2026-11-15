const $ = (id) => document.getElementById(id);
const state = { browsePath: null, browseParent: null, jobId: null, inspection: null };

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

async function openBrowser(path) {
  try {
    const data = await getJson(`/api/browse?path=${encodeURIComponent(path)}`);
    state.browsePath = data.path;
    state.browseParent = data.parent;
    $('browsePath').textContent = data.path;
    $('browserEntries').innerHTML = '';
    data.entries.forEach((entry) => {
      const row = document.createElement('div');
      row.className = 'browser-entry';
      const open = document.createElement('button');
      open.className = entry.directory ? 'entry-name directory' : 'entry-name';
      open.textContent = `${entry.directory ? '▸ ' : entry.image ? '▧ ' : '· '}${entry.name}`;
      if (entry.directory) open.onclick = () => openBrowser(entry.path);
      else if (entry.image) open.onclick = () => {
        $('backgroundSource').value = entry.path;
        preview(entry.path);
      };
      row.appendChild(open);
      if (entry.directory) {
        const use = document.createElement('button');
        use.className = 'button tiny';
        use.textContent = 'Use folder';
        use.onclick = () => {
          $('sourceRoot').value = entry.path;
          $('browser').classList.add('hidden');
          inspectSource();
        };
        row.appendChild(use);
      }
      $('browserEntries').appendChild(row);
    });
    $('browser').classList.remove('hidden');
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
  const small = ['R36SX', 'R36HD', 'GB350'].includes($('device').value);
  $('outputSpec').textContent = `Target output: ${small ? '640×480' : '854×480'} RGB backgrounds · ≤160×130 optimized icons`;
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
      $('result').textContent = `Packs staged under “${$('outputRoot').value}”. Review SOURCE-NOTES.txt before redistribution.`;
      $('result').className = 'result good';
      return;
    }
    if (job.state === 'error' || job.state === 'cancelled') {
      $('progressTitle').textContent = job.state === 'cancelled' ? 'Conversion cancelled' : 'Conversion needs attention';
      $('cancelButton').classList.add('hidden');
      $('result').textContent = job.error || 'The conversion did not complete.';
      $('result').className = 'result bad';
      return;
    }
    setTimeout(pollJob, 250);
  } catch (error) {
    $('log').textContent += `\n${error.message}`;
  }
}

async function cancelJob() {
  if (state.jobId) await postJson(`/api/jobs/${state.jobId}/cancel`, {});
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

document.querySelectorAll('.step').forEach((button) => button.addEventListener('click', () => showStep(button.dataset.step)));
$('browseButton').addEventListener('click', () => openBrowser($('sourceRoot').value.trim() || '/'));
$('parentButton').addEventListener('click', () => state.browseParent && openBrowser(state.browseParent));
$('inspectButton').addEventListener('click', inspectSource);
$('backgroundSource').addEventListener('change', (event) => preview(event.target.value));
$('convertButton').addEventListener('click', startConversion);
$('cancelButton').addEventListener('click', cancelJob);
$('dim').addEventListener('input', (event) => $('dimValue').textContent = `${event.target.value}%`);
$('quality').addEventListener('input', (event) => $('qualityValue').textContent = event.target.value);
$('device').addEventListener('change', updateOutputSpec);
$('mode').addEventListener('change', () => {
  const mode = $('mode').value;
  $('backgroundSource').disabled = mode === 'icons';
  $('iconRoot').disabled = mode === 'backgrounds';
});
checkDependencies();
updateOutputSpec();
