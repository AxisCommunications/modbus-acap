var params = {};
var scenarios = {};
const appname = 'Modbusacap';
const parambaseurl = '/axis-cgi/param.cgi?action=';
const paramgeturl = parambaseurl + 'list&group= ' + appname + '.';
const paramseturl = parambaseurl + 'update&' + appname + '.';
const scenariourl = '/local/objectanalytics/control.cgi';
const clientcfg = document.getElementById("clientcfg");
const table = document.getElementById("scenarios");
const address = document.getElementById("address");
const modeselection = document.getElementById("modeselection");
const port = document.getElementById("port");
const scenarioselection = document.getElementById("scenarioselection");
const server = document.getElementById("server");
const UpdateInterval = 5;
const SERVER = 0;
const CLIENT = 1;

function addressChange(newaddress) {
	setNewValue('ModbusAddress', newaddress);
}

function modeChange(newmode, setparam = true) {
	if (setparam) {
		setNewValue('Mode', newmode);
	}
	switch (+newmode) {
		case SERVER:
			clientcfg.style.display = 'none';
			break;
		case CLIENT:
			clientcfg.style.display = 'block';
			break;
		default:
			console.error('Unknown application mode: ' + newmode);
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

function updateScenarioTable() {
	for (let i = table.rows.length - 1; i > 0; i--) {
		table.deleteRow(i);
	}
	for (const scenario of scenarios) {
		const row = table.insertRow();
		var i = 0;
		const nameCell = row.insertCell(i++);
		const idCell = row.insertCell(i++);
		const typeCell = row.insertCell(i++);

		idCell.innerText = scenario.id;
		nameCell.innerText = scenario.name;
		typeCell.innerText = scenario.type.replace(/([A-Z])/g, ' $1').toLowerCase();
	}
}

function updateScenarioSelection() {
	while (scenarioselection.options.length > 0) {
		scenarioselection.remove(0);
	}
	for (const scenario of scenarios) {
		scenarioselection.add(new Option(`${scenario.name} (ID: ${scenario.id})`, scenario.id));
	}
	scenarioselection.value = +(params['Scenario']);
}

async function getScenarios() {
	try {
		const scenarioData = await $.post({
			url: scenariourl,
			headers: {
				'Content-Type': 'application/json'
			},
			data: '{"apiVersion": "1.2", "method": "getConfiguration"}'
		});
		scenarios = scenarioData.data.scenarios;
		for (i in scenarioData.data.scenarios) {
			console.log('scenario id: ' + scenarios[i].id + ', scenario name: ' + scenarios[i].name + ', type: ' + scenarios[i].type);
		}
		updateScenarioTable();
	} catch (error) {
		alert('Failed to get scenario info from AOA');
		console.error('Failed to get scenario info from AOA: ', error)
	}
}

async function getCurrentValue(param) {
	try {
		const paramData = await $.get(paramgeturl + param);
		params[param] = paramData.split('=')[1];
		console.log('Got ' + param + ' value ' + params[param]);
	} catch (error) {
		alert('Failed to get parameter ' + param);
	}
}

async function setNewValue(param, value) {
	try {
		await $.get(paramseturl + param + '=' + value);
		params[param] = value;
		console.log('Set ' + param + ' value to ' + value);
	} catch (error) {
		alert('Failed to set parameter ' + param);
	}
}

async function updateValues() {
	await getCurrentValue('ModbusAddress');
	await getCurrentValue('Mode');
	await getCurrentValue('Port');
	await getCurrentValue('Scenario');
	await getCurrentValue('Server');
	await getScenarios();
	modeChange(+(params['Mode']), false);
	modeselection.value = +(params['Mode']);
	address.value = +(params['ModbusAddress']);
	port.value = +(params['Port']);
	updateScenarioSelection();
	server.value = params['Server'];
	console.log('Will call again in ' + UpdateInterval + ' seconds to make sure we are in sync with device');
	setTimeout(updateValues, 1000 * UpdateInterval);
}

updateValues();
