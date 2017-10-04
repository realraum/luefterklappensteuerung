"use strict";

var webSocketUrl = 'ws://'+window.location.hostname+'/sock';

var webSocketSupport = null;

var wsctx_ventchange = "ventchange";
var wsctx_error = "error";
var wsctx_lock = "lock";

function handleExternalControlStateChange(data) {
	$(".controlstate").removeClass("active");
	Object.keys(data).forEach(function(name)  {
		if (data[name]) {
			$(".controlstate[name="+name+"][state="+data[name]+"]").addClass("active");
		}
	});
}

function sendControlStateUpdate() {
	var newstate={};
	$(".controlstate.active").each(function(elem){
		newstate[elem.getAttribute("name")]=elem.getAttribute("state");
	});
	ws.send(wsctx_ventchange, newstate);
}

function handleControlStateChange(event) {
	var mygrp = $(event.target).parent().children().removeClass("active");
	$(event.target).addClass("active");
	sendControlStateUpdate();
}

function displayErrorMsg(msg, duration) {
	$("#error-msg").text(msg);
	$("div.erroroverlay").css("display","table");
	setTimeout(function() {$("div.erroroverlay").css("display","none");}, duration);
}

function handleErrorMsg(data) {
	displayErrorMsg(data.msg, 1100);
}

function ShowWaitingForConnection() {
    $("div.waitingoverlay").css("display","initial");
}

function ShowConnectionEstablished() {
	$("div.waitingoverlay").css("display","none");
}

(function() {
  webSocketSupport = hasWebSocketSupport();

  //set background color for fancylightpresetbuttons according to ledr=, ledb=, etc.
  if (webSocketSupport) {
	$(".controlstate").on("click",handleControlStateChange);

    ws.registerContext(wsctx_ventchange,handleExternalControlStateChange);
    ws.registerContext(wsctx_error,handleErrorMsg);
    ws.onopen = ShowConnectionEstablished;
    ws.ondisconnect = ShowWaitingForConnection;
    ws.open(webSocketUrl);
  }
})();
