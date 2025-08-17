// Copyright (c) Craig Millard and contributors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//
// See the License for the specific language governing permissions and
// limitations under the License.

#include <webAssets.h>

String getConfigHTML() {
    return R"RAW(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Guitar Switcher Configuration</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <header>
            <h1>🎸 Guitar Switcher Configuration</h1>
            <div class="status-bar">
                <span id="connection-status">Connecting...</span>
                <span id="device-info">Loading...</span>
            </div>
        </header>

        <nav class="tabs">
            <button class="tab-button active" onclick="showTab('footswitches')">Footswitches</button>
            <button class="tab-button" onclick="showTab('scenes')">Scenes</button>
            <button class="tab-button" onclick="showTab('system')">System</button>
        </nav>

        <main>
            <div id="footswitches-tab" class="tab-content active">
                <div class="section">
                    <h2>Footswitch Mappings</h2>
                    <div class="config-toolbar">
                        <button onclick="addFootswitchMapping()" class="btn btn-primary">Add Mapping</button>
                        <button onclick="loadConfiguration()" class="btn btn-secondary">Reload</button>
                        <button onclick="saveConfiguration()" class="btn btn-success">Save All</button>
                    </div>
                    
                    <div id="footswitch-mappings" class="mappings-container">
                    </div>
                </div>
            </div>

            <div id="scenes-tab" class="tab-content">
                <div class="section">
                    <h2>Scene Management</h2>
                    <div class="config-toolbar">
                        <button onclick="addScene()" class="btn btn-primary">Add Scene</button>
                        <button onclick="captureCurrentState()" class="btn btn-warning">Capture Current</button>
                    </div>
                    
                    <div id="scenes-container" class="mappings-container">
                    </div>
                </div>
            </div>

            <div id="system-tab" class="tab-content">
                <div class="section">
                    <h2>System Information</h2>
                    <div id="system-info" class="info-grid">
                    </div>
                </div>

                <div class="section">
                    <h2>Configuration Management</h2>
                    <div class="config-toolbar">
                        <button onclick="exportConfiguration()" class="btn btn-info">Export Config</button>
                        <input type="file" id="import-file" accept=".json" style="display: none;" onchange="importConfiguration(event)">
                        <button onclick="document.getElementById('import-file').click()" class="btn btn-info">Import Config</button>
                        <button onclick="rebootDevice()" class="btn btn-danger">Reboot Device</button>
                    </div>
                </div>

                <div class="section">
                    <h2>Connected Devices</h2>
                    <div id="peer-list" class="peer-grid">
                    </div>
                </div>
            </div>
        </main>

        <div id="toast-container"></div>
    </div>

    <script src="/script.js"></script>
</body>
</html>)RAW";
}

String getConfigCSS() {
    return R"(
/* Guitar Switcher Configuration Styles */
:root {
    --primary-color: #2563eb;
    --secondary-color: #64748b;
    --success-color: #16a34a;
    --warning-color: #d97706;
    --danger-color: #dc2626;
    --info-color: #0891b2;
    --bg-color: #f8fafc;
    --card-color: #ffffff;
    --border-color: #e2e8f0;
    --text-color: #1e293b;
    --text-light: #64748b;
}

* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    background-color: var(--bg-color);
    color: var(--text-color);
    line-height: 1.6;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

header {
    background: var(--card-color);
    padding: 2rem;
    border-radius: 12px;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
    margin-bottom: 2rem;
}

header h1 {
    font-size: 2.5rem;
    font-weight: 700;
    margin-bottom: 1rem;
    color: var(--primary-color);
}

.status-bar {
    display: flex;
    gap: 2rem;
    font-size: 0.9rem;
    color: var(--text-light);
}

.tabs {
    display: flex;
    background: var(--card-color);
    border-radius: 12px;
    padding: 8px;
    margin-bottom: 2rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.tab-button {
    flex: 1;
    padding: 12px 24px;
    border: none;
    background: transparent;
    cursor: pointer;
    border-radius: 8px;
    font-weight: 500;
    transition: all 0.2s;
}

.tab-button.active {
    background: var(--primary-color);
    color: white;
}

.tab-button:hover:not(.active) {
    background: var(--bg-color);
}

.tab-content {
    display: none;
}

.tab-content.active {
    display: block;
}

.section {
    background: var(--card-color);
    border-radius: 12px;
    padding: 2rem;
    margin-bottom: 2rem;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.section h2 {
    font-size: 1.5rem;
    margin-bottom: 1.5rem;
    color: var(--text-color);
}

.config-toolbar {
    display: flex;
    gap: 1rem;
    margin-bottom: 2rem;
    flex-wrap: wrap;
}

.btn {
    padding: 10px 20px;
    border: none;
    border-radius: 8px;
    cursor: pointer;
    font-weight: 500;
    transition: all 0.2s;
    text-decoration: none;
    display: inline-block;
}

.btn-primary { background: var(--primary-color); color: white; }
.btn-secondary { background: var(--secondary-color); color: white; }
.btn-success { background: var(--success-color); color: white; }
.btn-warning { background: var(--warning-color); color: white; }
.btn-danger { background: var(--danger-color); color: white; }
.btn-info { background: var(--info-color); color: white; }

.btn:hover {
    opacity: 0.9;
    transform: translateY(-1px);
}

.mapping-card {
    border: 2px solid var(--border-color);
    border-radius: 12px;
    padding: 1.5rem;
    margin-bottom: 1rem;
    background: var(--bg-color);
}

.mapping-card.enabled {
    border-color: var(--success-color);
}

.mapping-header {
    display: flex;
    justify-content: between;
    align-items: center;
    margin-bottom: 1rem;
}

.mapping-title {
    font-weight: 600;
    font-size: 1.1rem;
}

.mapping-controls {
    display: flex;
    gap: 0.5rem;
}

.form-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
    gap: 1rem;
}

.form-group {
    margin-bottom: 1rem;
}

.form-group label {
    display: block;
    margin-bottom: 0.5rem;
    font-weight: 500;
    color: var(--text-color);
}

.form-group input,
.form-group select,
.form-group textarea {
    width: 100%;
    padding: 10px;
    border: 2px solid var(--border-color);
    border-radius: 6px;
    font-size: 1rem;
}

.form-group input:focus,
.form-group select:focus,
.form-group textarea:focus {
    outline: none;
    border-color: var(--primary-color);
}

.info-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
    gap: 1rem;
}

.info-item {
    display: flex;
    justify-content: space-between;
    padding: 1rem;
    background: var(--bg-color);
    border-radius: 8px;
}

.peer-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
    gap: 1rem;
}

.peer-card {
    padding: 1rem;
    background: var(--bg-color);
    border-radius: 8px;
    border: 1px solid var(--border-color);
}

.toast {
    position: fixed;
    top: 20px;
    right: 20px;
    padding: 1rem 1.5rem;
    border-radius: 8px;
    color: white;
    font-weight: 500;
    z-index: 1000;
    animation: slideIn 0.3s ease;
}

.toast.success { background: var(--success-color); }
.toast.error { background: var(--danger-color); }
.toast.info { background: var(--info-color); }

@keyframes slideIn {
    from { transform: translateX(100%); opacity: 0; }
    to { transform: translateX(0); opacity: 1; }
}

.test-button {
    padding: 6px 12px;
    font-size: 0.8rem;
    border-radius: 6px;
}

.mapping-actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
    justify-content: flex-end;
}

@media (max-width: 768px) {
    .container {
        padding: 10px;
    }
    
    header {
        padding: 1rem;
    }
    
    header h1 {
        font-size: 2rem;
    }
    
    .status-bar {
        flex-direction: column;
        gap: 0.5rem;
    }
    
    .config-toolbar {
        flex-direction: column;
    }
    
    .form-grid {
        grid-template-columns: 1fr;
    }
}
)";
}

String getConfigJS() {
    return R"RAW(
// Guitar Switcher Configuration JavaScript
let currentConfig = { mappings: [], scenes: [], settings: {} };
let systemStatus = {};

document.addEventListener('DOMContentLoaded', function() {
    showTab('footswitches');
    loadSystemStatus();
    loadConfiguration();
    setInterval(loadSystemStatus, 30000);
});

function showTab(tabName) {
    document.querySelectorAll('.tab-content').forEach(tab => {
        tab.classList.remove('active');
    });
    document.querySelectorAll('.tab-button').forEach(btn => {
        btn.classList.remove('active');
    });
    
    document.getElementById(tabName + '-tab').classList.add('active');
    event.target.classList.add('active');
}

async function loadSystemStatus() {
    try {
        const response = await fetch('/api/status');
        systemStatus = await response.json();
        updateStatusDisplay();
        updateSystemInfo();
        updatePeerList();
        document.getElementById('connection-status').textContent = 'Connected';
        document.getElementById('connection-status').style.color = 'green';
    } catch (error) {
        document.getElementById('connection-status').textContent = 'Disconnected';
        document.getElementById('connection-status').style.color = 'red';
        showToast('Failed to load system status', 'error');
    }
}

function updateStatusDisplay() {
    const deviceInfo = systemStatus.deviceName + ' v' + systemStatus.firmwareVersion;
    document.getElementById('device-info').textContent = deviceInfo;
}

function updateSystemInfo() {
    const infoContainer = document.getElementById('system-info');
    const uptime = formatUptime(systemStatus.uptime);
    
    infoContainer.innerHTML = 
        '<div class="info-item"><span>Firmware:</span><span>' + systemStatus.firmwareVersion + '</span></div>' +
        '<div class="info-item"><span>Device:</span><span>' + systemStatus.deviceName + '</span></div>' +
        '<div class="info-item"><span>Uptime:</span><span>' + uptime + '</span></div>' +
        '<div class="info-item"><span>Memory:</span><span>' + systemStatus.freeHeap + ' bytes</span></div>' +
        '<div class="info-item"><span>Channel:</span><span>' + systemStatus.espnowChannel + '</span></div>' +
        '<div class="info-item"><span>Peers:</span><span>' + systemStatus.connectedPeers + '</span></div>';
}

function updatePeerList() {
    const peerContainer = document.getElementById('peer-list');
    
    if (!systemStatus.peers || systemStatus.peers.length === 0) {
        peerContainer.innerHTML = '<p>No connected devices</p>';
        return;
    }
    
    let peerHTML = '';
    for (let i = 0; i < systemStatus.peers.length; i++) {
        const peer = systemStatus.peers[i];
        peerHTML += '<div class="peer-card"><h4>' + peer.name + '</h4><p>MAC: ' + peer.mac + '</p></div>';
    }
    peerContainer.innerHTML = peerHTML;
}

async function loadConfiguration() {
    try {
        const response = await fetch('/api/config');
        currentConfig = await response.json();
        renderFootswitchMappings();
        renderScenes();
        showToast('Configuration loaded', 'success');
    } catch (error) {
        showToast('Failed to load configuration', 'error');
    }
}

async function saveConfiguration() {
    try {
        const response = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(currentConfig)
        });
        
        if (response.ok) {
            showToast('Configuration saved successfully', 'success');
        } else {
            throw new Error('Save failed');
        }
    } catch (error) {
        showToast('Failed to save configuration', 'error');
    }
}

function renderFootswitchMappings() {
    const container = document.getElementById('footswitch-mappings');
    
    if (!currentConfig.mappings || currentConfig.mappings.length === 0) {
        container.innerHTML = '<p>No footswitch mappings configured. Click "Add Mapping" to get started.</p>';
        return;
    }
    
    let mappingsHTML = '';
    for (let i = 0; i < currentConfig.mappings.length; i++) {
        mappingsHTML += createMappingCard(currentConfig.mappings[i], i);
    }
    container.innerHTML = mappingsHTML;
}

function createMappingCard(mapping, index) {
    const enabledClass = mapping.enabled ? 'enabled' : '';
    const checkedAttr = mapping.enabled ? 'checked' : '';
    
    return '<div class="mapping-card ' + enabledClass + '">' +
        '<div class="mapping-header">' +
        '<div class="mapping-title">Footswitch ' + (mapping.footswitchIndex + 1) + '</div>' +
        '<div class="mapping-controls">' +
        '<button class="btn btn-info test-button" onclick="testMapping(' + index + ')">Test</button>' +
        '<button class="btn btn-danger test-button" onclick="removeMapping(' + index + ')">Remove</button>' +
        '</div></div>' +
        '<div class="form-grid">' +
        '<div class="form-group">' +
        '<label>Enabled</label>' +
        '<input type="checkbox" ' + checkedAttr + ' onchange="updateMappingField(' + index + ', \'enabled\', this.checked)">' +
        '</div>' +
        '<div class="form-group">' +
        '<label>Description</label>' +
        '<input type="text" value="' + (mapping.description || '') + '" onchange="updateMappingField(' + index + ', \'description\', this.value)">' +
        '</div></div></div>';
}

function addFootswitchMapping() {
    const newMapping = {
        footswitchIndex: currentConfig.mappings.length,
        actionType: 0,
        targetChannel: 0,
        enabled: true,
        description: 'Footswitch ' + (currentConfig.mappings.length + 1)
    };
    
    currentConfig.mappings.push(newMapping);
    renderFootswitchMappings();
}

function removeMapping(index) {
    currentConfig.mappings.splice(index, 1);
    renderFootswitchMappings();
}

function updateMappingField(index, field, value) {
    currentConfig.mappings[index][field] = value;
    renderFootswitchMappings();
}

async function testMapping(index) {
    try {
        const response = await fetch('/api/test', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                footswitch: currentConfig.mappings[index].footswitchIndex,
                longPress: false
            })
        });
        
        if (response.ok) {
            showToast('Test action executed', 'success');
        } else {
            throw new Error('Test failed');
        }
    } catch (error) {
        showToast('Test action failed', 'error');
    }
}

function renderScenes() {
    const container = document.getElementById('scenes-container');
    
    if (!currentConfig.scenes || currentConfig.scenes.length === 0) {
        container.innerHTML = '<p>No scenes configured.</p>';
        return;
    }
    
    let scenesHTML = '';
    for (let i = 0; i < currentConfig.scenes.length; i++) {
        scenesHTML += createSceneCard(currentConfig.scenes[i], i);
    }
    container.innerHTML = scenesHTML;
}

function createSceneCard(scene, index) {
    const enabledClass = scene.enabled ? 'enabled' : '';
    const checkedAttr = scene.enabled ? 'checked' : '';
    
    return '<div class="mapping-card ' + enabledClass + '">' +
        '<div class="mapping-header">' +
        '<div class="mapping-title">' + scene.sceneName + '</div>' +
        '<div class="mapping-controls">' +
        '<button class="btn btn-info test-button" onclick="recallScene(' + index + ')">Recall</button>' +
        '<button class="btn btn-danger test-button" onclick="removeScene(' + index + ')">Remove</button>' +
        '</div></div>' +
        '<div class="form-grid">' +
        '<div class="form-group">' +
        '<label>Scene Name</label>' +
        '<input type="text" value="' + scene.sceneName + '" onchange="updateSceneField(' + index + ', \'sceneName\', this.value)">' +
        '</div>' +
        '<div class="form-group">' +
        '<label>Enabled</label>' +
        '<input type="checkbox" ' + checkedAttr + ' onchange="updateSceneField(' + index + ', \'enabled\', this.checked)">' +
        '</div></div></div>';
}

function addScene() {
    const newScene = {
        sceneIndex: currentConfig.scenes.length,
        sceneName: 'Scene ' + (currentConfig.scenes.length + 1),
        relayStates: [false, false, false, false],
        enabled: true
    };
    
    currentConfig.scenes.push(newScene);
    renderScenes();
}

function removeScene(index) {
    currentConfig.scenes.splice(index, 1);
    renderScenes();
}

function updateSceneField(index, field, value) {
    currentConfig.scenes[index][field] = value;
    renderScenes();
}

function captureCurrentState() {
    if (systemStatus.relayStates) {
        const newScene = {
            sceneIndex: currentConfig.scenes.length,
            sceneName: 'Captured Scene ' + new Date().toLocaleTimeString(),
            relayStates: systemStatus.relayStates.slice(),
            enabled: true
        };
        
        currentConfig.scenes.push(newScene);
        renderScenes();
        showToast('Current state captured', 'success');
    }
}

async function recallScene(index) {
    showToast('Scene "' + currentConfig.scenes[index].sceneName + '" recalled', 'info');
}

function exportConfiguration() {
    const dataStr = JSON.stringify(currentConfig, null, 2);
    const dataBlob = new Blob([dataStr], {type: 'application/json'});
    const url = URL.createObjectURL(dataBlob);
    
    const link = document.createElement('a');
    link.href = url;
    link.download = 'guitar_switcher_config.json';
    link.click();
    
    URL.revokeObjectURL(url);
    showToast('Configuration exported', 'success');
}

async function importConfiguration(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    try {
        const text = await file.text();
        const importedConfig = JSON.parse(text);
        
        if (importedConfig.mappings || importedConfig.scenes) {
            currentConfig = importedConfig;
            renderFootswitchMappings();
            renderScenes();
            showToast('Configuration imported successfully', 'success');
        } else {
            throw new Error('Invalid configuration file');
        }
    } catch (error) {
        showToast('Failed to import configuration', 'error');
    }
}

function formatUptime(ms) {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);
    
    if (days > 0) return days + 'd ' + (hours % 24) + 'h ' + (minutes % 60) + 'm';
    if (hours > 0) return hours + 'h ' + (minutes % 60) + 'm';
    if (minutes > 0) return minutes + 'm ' + (seconds % 60) + 's';
    return seconds + 's';
}

function showToast(message, type) {
    if (!type) type = 'info';
    const toast = document.createElement('div');
    toast.className = 'toast ' + type;
    toast.textContent = message;
    
    document.getElementById('toast-container').appendChild(toast);
    
    setTimeout(function() {
        toast.remove();
    }, 3000);
}

async function rebootDevice() {
    if (confirm('Are you sure you want to reboot the device?')) {
        try {
            await fetch('/api/reboot', { method: 'POST' });
            showToast('Device rebooting...', 'info');
        } catch (error) {
            showToast('Failed to reboot device', 'error');
        }
    }
}
)RAW";
}
