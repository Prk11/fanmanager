async function onSpeedChange() {
    var speed = document.getElementById("fanspeed").value;
    const requestFan = new Request("/fan?speed=" + speed);
    const responseFan = await fetch(requestFan);
    const jsonFan = await responseFan.json();
    var fanstatus = document.getElementById("fanstatus");
    if (speed > 0) {
        fanstatus.innerText = speed + " скорость";
    } else {
        fanstatus.innerText = "Выкл.";
    }
    let currentspeed = document.getElementById("currentspeed");
    if (responseFan.status == 200) {
        currentspeed.innerText = jsonFan["speed"] + " скорость";
        return jsonFan["speed"];
        //status.className = "";
    } else {
        currentspeed.innerText = "Возникла не предвиденая ошибка. Перегрузите устройство!";
        return false;
        //status.className = "alert-danger";
    }
}

function fanspeedchange() {
    return onSpeedChange();
}

async function getInfo() {
    const requestInfo = new Request("/info");
    const responseInfo = await fetch(requestInfo);
    const jsonInfo = await responseInfo.json();

    forfirstload = document.getElementById("forfirstload");

    document.getElementById("chipId").innerText = jsonInfo["chipId"];
    document.getElementById("MAC").innerText = jsonInfo["MAC"];
    document.getElementById("IP").innerText = jsonInfo["IP"];
    document.getElementById("chipModel").innerText = jsonInfo["chipModel"];
    document.getElementById("chipRevision").innerText = jsonInfo["chipRevision"];
    document.getElementById("sdkVersion").innerText = jsonInfo["sdkVersion"];
    document.getElementById("cpuCore").innerText = jsonInfo["cpuCore"];
    document.getElementById("flashSize").innerText = jsonInfo["flashSize"];
    document.getElementById("flashSpeed").innerText = jsonInfo["flashSpeed"];
    document.getElementById("cpuSpeed").innerText = jsonInfo["cpuSpeed"];
    document.getElementById("wifiMode").innerText = jsonInfo["wifiMode"];
    document.getElementById("RSSI").innerText = jsonInfo["RSSI"];
    document.getElementById("SSID").innerText = jsonInfo["SSID"];
    document.getElementById("fsTotalSize").innerText = jsonInfo["fsTotalSize"];
    document.getElementById("fsUsedSize").innerText = jsonInfo["fsUsedSize"];
    document.getElementById("fsFreeSize").innerText = jsonInfo["fsFreeSize"];
    var haButton = document.getElementById("haButton");
    if (jsonInfo["haStatus"]) {
        haButton.classList.replace("btn-danger", "btn-success");
        document.getElementById("haStatus").innerText = "Подключено";
    }
    else {
        haButton.classList.replace("btn-success", "btn-danger");
        document.getElementById("haStatus").innerText = "Не подключено";
    }

    if (forfirstload.value == 1) {
        document.getElementById("wifissid").value = jsonInfo["SSID"];

        const requestConfig = new Request("/config.json");
        const responseConfig = await fetch(requestConfig);
        const jsonConfig = await responseConfig.json();

        document.getElementById("pin4").value = jsonConfig["GPIOButton"];
        document.getElementById("pin1").value = jsonConfig["GPIOSpeed1"];
        document.getElementById("pin2").value = jsonConfig["GPIOSpeed2"];
        document.getElementById("pin3").value = jsonConfig["GPIOSpeed3"];

        document.getElementById("haenable").checked = jsonConfig["haEnable"];
        document.getElementById("haserver").value = jsonConfig["haServer"];
        document.getElementById("halogin").value = jsonConfig["haLogin"];
        document.getElementById("hapassword").value = jsonConfig["haPassword"];

        if (jsonConfig["haEnable"]) {
            document.getElementById("haserver").readOnly = false;
            document.getElementById("halogin").readOnly = false;
            document.getElementById("hapassword").readOnly = false;
        }
        else {
            document.getElementById("haserver").readOnly = true;
            document.getElementById("halogin").readOnly = true;
            document.getElementById("hapassword").readOnly = true;
        }

        getFilesList() ;
    }

    const requestFan = new Request("/fan");
    const responseFan = await fetch(requestFan);
    const jsonFan = await responseFan.json();
    let fanspeedblock = document.getElementById("fanspeedblock").value;
    if (fanspeedblock==0)
        document.getElementById("fanspeed").value = jsonFan["speed"];
    fanstatus = document.getElementById("fanstatus");
    currentspeed = document.getElementById("currentspeed");
    fanspeed = document.getElementById("fanspeed");
    if (jsonFan["speed"] > 0) {
        fanstatus.innerText = jsonFan["speed"] + " скорость";
        currentspeed.innerText = jsonFan["speed"] + " скорость";
    } else {
        fanstatus.innerText = "Выкл.";
        currentspeed.innerText = "Выкл.";
    }

    forfirstload.value = 0;
}

async function sendPins() {

    var pinsButton = document.getElementById("pinsButton");

    pinsButton.className = "btn btn-success disabled";

    var pin4 = document.getElementById("pin4").value;
    var pin1 = document.getElementById("pin1").value;
    var pin2 = document.getElementById("pin2").value;
    var pin3 = document.getElementById("pin3").value;

    var requestJson = '{"GPIOButton": ' + pin4 +
        ',"GPIOSpeed1": ' + pin1 +
        ',"GPIOSpeed2": ' + pin2 +
        ',"GPIOSpeed3": ' + pin3 + '}';

    const requestPins = new Request("/pins", {
        method: "POST",
        body: requestJson,
    });
    const resposePins = await fetch(requestPins);

    pinsButton.classList.remove("disabled");
    if (resposePins.status != 200) {
        pinsButton.classList.replace("btn-success", "btn-danger");
    }

}

function onSendPins() {
    sendPins();
}

async function HandlerHAParameters() {

    var haButton = document.getElementById("haButton");

    haButton.className = "btn btn-success disabled";

    var haEnable = document.getElementById("haenable").checked;
    var haServer = document.getElementById("haserver").value;
    var haLogin = document.getElementById("halogin").value;
    var haPassword = document.getElementById("hapassword").value;

    var requestJson = '{"haEnable": ' + haEnable +
        ',"haServer": "' + haServer + '"' +
        ',"haLogin": "' + haLogin + '"' +
        ',"haPassword": "' + haPassword + '"}';

    const requestHa = new Request("/ha", {
        method: "POST",
        body: requestJson,
    });
    const resposeHa = await fetch(requestHa);

    haButton.classList.remove("disabled");
    if (resposeHa.status != 200) {
        haButton.classList.replace("btn-success", "btn-danger");
    } else {
        haButton.classList.replace("btn-danger", "btn-success");
    }

}

function onHandlerHAParameters() {
    const ipv4Pattern = /^(\d{1,3}\.){3}\d{1,3}$/;
    var haServer = document.getElementById("haserver");
    if (!ipv4Pattern.test(haServer.value)) {
        haServer.focus();
        alert("Формат поля не соотвествует IPv4!");
        return false;
    }

    HandlerHAParameters();
}

function onHAEnable() {
    var haEnable = document.getElementById("haenable");
    var haServer = document.getElementById("haserver");
    var haLogin = document.getElementById("halogin");
    var haPassword = document.getElementById("hapassword");

    if (haEnable.checked) {
        haServer.readOnly = false;
        haLogin.readOnly = false;
        haPassword.readOnly = false;
    }
    else {
        haServer.readOnly = true;
        haLogin.readOnly = true;
        haPassword.readOnly = true;
    }
}

async function getFilesList() {   
    const requestInfo = new Request("/list");
    const responseInfo = await fetch(requestInfo);
    const jsonList = await responseInfo.json();
    let filelist = document.getElementById("filelist");
    let tbody = filelist.getElementsByTagName("tbody")[0];
    tbody.innerHTML ="";
    let filesize = document.getElementById("filesize");
    let freesize = document.getElementById("freesize");

    filesize.innerText = jsonList["filesize"];
    freesize.innerText = jsonList["freesize"];

    const jsonFilesList = jsonList["filelist"];
    for (let index = 0; index < jsonFilesList.length; index++) {
        const element = jsonFilesList[index];
        let tr = document.createElement("tr");
        tbody.appendChild(tr);
        let tdNum = document.createElement("td");
        tdNum.innerText = index+1;
        tr.appendChild(tdNum);
        let tdName = document.createElement("td");
        let fileIcon = (element["type"]=="file")?'<i class="bi bi-file-earmark-text"></i> ':'<i class="bi bi-folder"></i> ';
        tdName.innerHTML =  fileIcon + ' <a href="/'+element["name"]+'">'+element["name"]+'<\a>';
        tr.appendChild(tdName);
        let tdSize = document.createElement("td");
        tdSize.innerText = element["size"];
        tr.appendChild(tdSize);
        let tdAction = document.createElement("td");
        tdAction.innerHTML = '<button type="button" class="btn btn-outline-secondary" onclick="onFileDelete('+"'"+element["name"]+"'"+');"><i class="bi bi-trash"></i></button> ';
        tr.appendChild(tdAction);
    }
}

function onFilesList(){
    getFilesList() ;
}

async function doFileDelete(filename) {
    const requestDelete = new Request("/delete", {
        method: "DELETE",
        body: '/'+filename
    });
    const responseDelete = await fetch(requestDelete);
    const status = await responseDelete.status;
}

function onFileDelete(filename) {
    doFileDelete(filename.trim());
    onFilesList();
}

async function doFileUpload() {
    let fileUpload = document.getElementById("fileUpload");
    let fileNameUpload = document.getElementById("fileNameUpload");
    if (fileUpload.files.length!=1) {
        alert('Выберите файл для загрузки');
        return;
    }
    if (fileNameUpload.value=="") {
        alert('Имя файла не должно быть пустым');
        return;
    }
    let formData = new FormData();
    formData.append("filename", fileNameUpload.value);
    formData.append("upload", fileUpload.files[0]);
    const requestUpload = new Request("/upload", {method: "POST", body: formData});
    const responseUpload = await fetch(requestUpload);
    if (responseUpload.status == 200) {
        fileUpload.value = null;
        let fileUploadForm = document.getElementById("fileUploadForm");
        fileUploadForm.className.replace('show', ' ');
        fileUploadForm.style.display = 'none';
        fileUploadForm.removeAttribute('aria-modal');
        fileUploadForm.setAttribute('aria-hidden','true');
        let divs = document.getElementsByClassName('modal-backdrop fade show');
        if (divs.length==1) divs[0].remove();
        document.body.style = "";
        document.body.className = "";
        fileNameUpload.value ="";
        getFilesList();
    }
}

function onFileUpload() {
    doFileUpload();
}

function onSelectFile() {
    let fileUpload = document.getElementById("fileUpload");
    let fileNameUpload = document.getElementById("fileNameUpload");

    if (fileUpload.value) {
        var startIndex = (fileUpload.value.indexOf('\\') >= 0 ? fileUpload.value.lastIndexOf('\\') : fileUpload.value.lastIndexOf('/'));
        var filename = fileUpload.value.substring(startIndex);
        if (filename.indexOf('\\') === 0 || filename.indexOf('/') === 0) {
            filename = filename.substring(1);
        }
        fileNameUpload.value = filename;
    }
}

async function doSetWifi() {
    let wifissid = document.getElementById("wifissid");
    let wifipassword = document.getElementById("wifipassword");

    if (wifissid.value!="") {
        let formData = new FormData();
        formData.append("SSID",wifissid.value);
        formData.append("Password",wifipassword.value);
        const requestWifi = new Request("/wifi", {method: "POST", body: formData});
        const responseWifi = await fetch(requestWifi);
        if (responseWifi.status!=200) {
            wifipassword.value="";
            alert("Не удалось подключиться к WiFi сети. Проверьте правильность введеных данных");
            wifipassword.focus();
        }
    }
    else {
        alert('Необходимо указать параметры WiFi сети, к которой хотите подключится');
        wifissid.focus();
    }
}

function onSetWifi() {
    doSetWifi();
}

function onFanSpeedMouseOver(){
    fanspeedblock = document.getElementById("fanspeedblock");
    fanspeedblock.value = 1;
}

function onFanSpeedMouseOut(){
    fanspeedblock = document.getElementById("fanspeedblock");
    fanspeedblock.value = 0;
}
