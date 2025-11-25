const appname = 'Modbusacap';
const parambaseurl = '/axis-cgi/param.cgi?action=';
const paramgeturl = `${parambaseurl}list&group=${appname}.`;
const paramseturl = `${parambaseurl}update&${appname}.`;
const scenariourl = '/local/objectanalytics/control.cgi';
const updateInterval = 5000; // milliseconds
const MODE = { SERVER: 0, CLIENT: 1 };

let params = {};
let scenarios = [];

// Cache DOM elements
const elements = {
	clientcfg: document.getElementById('clientcfg'),
	table: document.getElementById('scenarios'),
	address: document.getElementById('address'),
	modeselection: document.getElementById('modeselection'),
	port: document.getElementById('port'),
	scenarioselection: document.getElementById('scenarioselection'),
	server: document.getElementById('server')
};

// Utility function for fetch with error handling
async function fetchWithErrorHandling(url, options = {}) {
	const response = await fetch(url, options);
	if (!response.ok) {
		throw new Error(`HTTP ${response.status}: ${response.statusText}`);
	}
	return response;
}

async function getCurrentValue(param) {
    try {
        const response = await fetchWithErrorHandling(paramgeturl + param);
        const paramData = await response.text();
        const value = paramData.split('=')[1]?.trim();
        
        if (undefined !== value) {
            params[param] = value;
            console.log(`Got ${param}: ${value}`);
            return value;
        }
        throw new Error('Invalid response format');
    } catch (error) {
        console.error(`Failed to get parameter ${param}:`, error);
        alert(`Failed to get parameter ${param}`);
    }
}

async function setNewValue(param, value) {
	try {
		await fetchWithErrorHandling(`${paramseturl}${param}=${encodeURIComponent(value)}`);
		params[param] = value;
		console.log(`Set ${param} to ${value}`);
	} catch (error) {
		console.error(`Failed to set parameter ${param}:`, error);
		alert(`Failed to set parameter ${param}`);
	}
}

async function getScenarios() {
	try {
		const response = await fetchWithErrorHandling(scenariourl, {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ apiVersion: '1.2', method: 'getConfiguration' })
		});

		const scenarioData = await response.json();
		scenarios = scenarioData.data?.scenarios || [];

		scenarios.forEach(scenario => {
			console.log(`Scenario ID: ${scenario.id}, Name: ${scenario.name}, Type: ${scenario.type}`);
		});

		updateScenarioTable();
		updateScenarioSelection();
	} catch (error) {
		console.error('Failed to get scenarios:', error);
		alert('Failed to get scenario info from AOA');
	}
}

function updateScenarioTable() {
	// Clear existing rows (keep header)
	while (1 < elements.table.rows.length) {
		elements.table.deleteRow(1);
	}

	// Add scenario rows
	scenarios.forEach(scenario => {
		const row = elements.table.insertRow();
		let i = 0;
		row.insertCell(i++).textContent = scenario.name;
		row.insertCell(i++).textContent = scenario.id;
		row.insertCell(i).textContent = formatScenarioType(scenario.type);
	});
}

function updateScenarioSelection() {
	// Clear options
	elements.scenarioselection.innerHTML = '';

	// Add new options
	scenarios.forEach(scenario => {
		const option = new Option(`${scenario.name} (ID: ${scenario.id})`, scenario.id);
		elements.scenarioselection.add(option);
	});

	// Set current value
	if (params.Scenario) {
		elements.scenarioselection.value = params.Scenario;
	}
}

function formatScenarioType(type) {
	return type.replace(/([A-Z])/g, ' $1').trim().toLowerCase();
}

function addressChange(newaddress) {
	setNewValue('ModbusAddress', newaddress);
}

function modeChange(newmode, setparam = true) {
	const mode = parseInt(newmode, 10);

	if (setparam) {
		setNewValue('Mode', mode);
	}

	elements.clientcfg.style.display = mode === MODE.CLIENT ? 'block' : 'none';

	if (mode !== MODE.SERVER && mode !== MODE.CLIENT) {
		console.error('Unknown application mode:', mode);
	}
}

function portChange(newport) {
	setNewValue('Port', newport);
}

function scenarioChange(newscenario) {
	setNewValue('Scenario', newscenario);
}

function serverChange(newserver) {
	setNewValue('Server', newserver);
}

function updateUI() {
	if (undefined !== params.Mode ) {
		elements.modeselection.value = params.Mode;
		modeChange(params.Mode, false);
	}

	if (undefined !== params.ModbusAddress) {
		elements.address.value = params.ModbusAddress;
	}

	if (undefined !== params.Port) {
		elements.port.value = params.Port;
	}

	if (undefined !== params.Server) {
		elements.server.value = params.Server;
	}
}

async function updateValues() {
	try {
		// Fetch all parameters in parallel
		await Promise.all([
			getCurrentValue('ModbusAddress'),
			getCurrentValue('Mode'),
			getCurrentValue('Port'),
			getCurrentValue('Scenario'),
			getCurrentValue('Server'),
			getScenarios()
		]);

		// Update UI with fetched values
		updateUI();

		console.log(`Will update again in ${updateInterval / 1000} seconds`);
	} catch (error) {
		console.error('Error updating values:', error);
	} finally {
		// Schedule next update
		setTimeout(updateValues, updateInterval);
	}
}

updateValues();
