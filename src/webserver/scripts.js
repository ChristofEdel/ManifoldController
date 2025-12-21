//////////////////////////////////////////////////////////////////////////////////////////////
//
// General helper functions
//
function fmt(value, digits, sign) {
    var x = Number(value).toFixed(digits)
    if (x == "NaN") return value; // probably non-numerical text, return 1:1
    // Number, prepend + sign if requested
    if (sign === "+" && x > 0) return "+" + x
    return x
}

///////////////////////////////////////////////////////////////////////////////////////////////
//
// Drag - drop support
//
function fixedWidthHelper(e, tr) {
    const $orig = tr.children()
    const $helper = tr.clone()
    $helper.children().each(function (i) {
        $(this).width($orig.eq(i).width())
    })
    return $helper
}

// Pit a sequence number (starting from 1) into the first <td> with class "seq"
// of every row (except for rows with class "template-row")
function renumberTable($table) {
    $table.find("tbody tr").not(".template-row").each(function (i) {
        const $first = $(this).children("td").eq(0)
        if (!$first.hasClass("seq")) return;  // skip this row
        $first.text(i + 1)
    })
}


/////////////////////////////////////////////////////////////////////////////////////////////
//
// Config page: Valve reset function
//

var zoneIdToReset = null
var zoneResetIntervalTimer = null
var zoneResetElapsed

// Step 1 - user selects a zone to reset.
//
// This disables the select field and obtains the current zone status from the server.
function zoneToResetSelected () {
    zoneIdToReset = $("#zoneToResetSelect").val() || null
    if (zoneIdToReset == "") zoneIdToReset = null; else zoneIdToReset = Number(zoneIdToReset)

    // No zone selected - we restore the initial state
    if (!zoneIdToReset) {
        $('#resetZoneStartButton').prop('disabled', true)
        $('#resetZoneStopButton').hide()
        $('#resetZoneSetpoint').text('')
        $('#resetZoneSetpoint').hide()
        $('#resetProgressIndicator').hide()
        return
    }

    // Zone selected - we get the zone setpoint and status, and once
    // we have it we enable the "Start Reset Sequence" button
    $.ajax({
        url: '/command',
        method: 'POST',
        contentType: 'application/json',
        data: JSON.stringify({command: 'GetZoneStatus', zoneId: zoneIdToReset }),
        success: function (data) {
            handleStatusResponse(data)
            $('#resetZoneSetpoint').show()
            $('#resetZoneStartButton').prop('disabled', false)
        }
    })
}

// Step 2 - user clicks the "Start Reset Sequence" button
function startResetSequence() {
    if (!zoneIdToReset) return
    
    // if we alredy have the interval timer runnimg, we do nothing
    if (zoneResetIntervalTimer) return
    $('#zoneToResetSelect').prop('disabled', true)
    $('#resetZoneStartButton').hide()
    $('#resetZoneStopButton').prop('disabled', false)
    $('#resetZoneStopButton').show()


    zoneResetElapsed = 0

    $('#resetProgressIndicator').css('left', zoneResetElapsed*2 + 'px')
    $('#resetProgressIndicator').show()

    var lastCommandSent = "";
    var lastCommandSentAt = 0; 

    zoneResetIntervalTimer = setInterval(function () {
        zoneResetElapsed++

        if (zoneResetElapsed <= 205) {
            $('#resetProgressIndicator').css('left', zoneResetElapsed*2 + 'px')
        }

        if (zoneResetElapsed <= 30) {
            // (a) 30 seconds ZoneOn
            cmd = 'ZoneOn'
        } else if (zoneResetElapsed <= 45) { // +15
            // (b) 15 seconds ZoneOff
            cmd = 'ZoneOff'
        } else if (zoneResetElapsed <= 60) { // +15
            // (c) 15 seconds ZoneOn
            cmd = 'ZoneOn'
        } else if (zoneResetElapsed <= 75) { // +15
            // (d) 15 seconds ZoneOff
            cmd = 'ZoneOff'
        } else if (zoneResetElapsed <= 165) { // +90
            // (d) 90 seconds ZoneOn
            cmd = 'ZoneOn'
        } else if (zoneResetElapsed <= 185) { // +20
            // (d) 20 seconds ZoneOff
            cmd = 'ZoneOff'
        } else if (zoneResetElapsed == 205) { // +20
            // (e) then set the zone back to automatic
            cmd = 'ZoneAuto'
        } else {
            // after that, get the status until we are finished
            cmd = 'GetZoneStatus'
        }

        if (cmd != lastCommandSent || zoneResetElapsed - lastCommandSentAt >= 5 ) {
            sendZoneCommand(zoneIdToReset, cmd)
            lastCommandSentAt = zoneResetElapsed
            lastCommandSent = cmd
        }

        if (zoneResetElapsed === 205) {
            clearInterval(zoneResetIntervalTimer)
            zoneResetIntervalTimer = null
            // End of sequence: show Reset button again
            $('#zoneToResetSelect').prop('disabled', false)
            $('#resetZoneStartButton').show()
            $('#resetZoneStopButton').hide()
            $('#resetProgressIndicator').hide()
        }
    }, 1000)
}

// User abort - skip forward to the final stage where the zone is set to 
// automatic and then monitored for 10 seconds
function stopResetSequence() {

    // Jump forward in the sequence so the next command is to set the zone to auto
    if (zoneResetElapsed < 105) zoneResetElapsed = 195

    // Just to make sure we send one ourselves
    sendZoneCommand(zoneIdToReset, 'ZoneAuto')

    $('#resetZoneStopButton').prop('disabled', true)
}

// Helper - whenever we receive the status of the zone, we put it
// in the "resetZoneSetpoint" element
function handleStatusResponse(data) {
    if (data && typeof data.setpoint !== 'undefined') {
        var setpoint = parseFloat(data.setpoint)
        if (!isNaN(setpoint)) {
            $('#resetZoneSetpoint').text(setpoint.toFixed(1))
        }
        else {
            $('#resetZoneSetpoint').text('???')
        }
    }

    if (data && typeof data.on !== 'undefined') {
        if (data.on) {
            $('#resetZoneSetpoint').addClass('zone-on')
            $('#resetZoneSetpoint').removeClass('zone-off')
        } else {
            $('#resetZoneSetpoint').removeClass('zone-on')
            $('#resetZoneSetpoint').addClass('zone-off')
        }
    }
}

// Helper - send a command for the zone.
function sendZoneCommand(zoneId, commandName) {
    $.ajax({
        url: '/command',
        method: 'POST',
        contentType: 'application/json',
        data: JSON.stringify({command: commandName, zoneId: zoneId }),
        success: function (data) {
            handleStatusResponse(data)
        }
    })
}


/////////////////////////////////////////////////////////////////////////////////////////////
//
// Config page: Manual valve control
//

function sendCommand(payload) {
  $.ajax({
    url: '/command',
    method: 'POST',
    contentType: 'application/json',
    data: JSON.stringify(payload),
    success: function (response) {
      if (response && response.reload === true) {
        location.reload()
      }
    }
  })
}

function valveControlManualCheckboxChanged() {
    var isChecked = $('#valveControlManualCheckbox').is(':checked')
    var automatic = !isChecked; // true if unchecked, false if checked

    var value = parseInt($('valveControlPositionSlider').val(), 10) || 0
    $('valveControlPositionText').text(value)

    sendCommand({
        command: 'SetValvePosition',
        parameters: {
            automatic: automatic,
            position: value
        }
    })
}

function valveControlSliderChanged() {

    var value = parseInt($('#valveControlPositionSlider').val(), 10) || 0
    $('#valveControlPositionText').text(value)

    // Ensure checkbox is checked; do not trigger its AJAX handler
    if (!$('#valveControlManualCheckbox').is(':checked')) {
        $('#valveControlManualCheckbox').prop('checked', true).triggerHandler('change')
    }

    sendCommand({
        command: 'SetValvePosition',
        parameters: {
            automatic: false,
            position: value
        }
    })
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//
// "Set value" command and dialog
//

function sendSetValueCommand(command, value) {
    var value = document.getElementById("newValue").value

    $.ajax({
        url: "/command",
        type: "POST",
        contentType: 'application/json',
        data: JSON.stringify({ command: command, value: value }),
        success: function () {
            $("#changeDialog").dialog("close")
            monitorPage_refreshData()
        },
        error: function () {
            alert("Error sending command")
        }
    })
}


function openSetValueDialog(element, title, command) {
    $("#newValue").val("")

    $("#changeDialog")
        .dialog("option", "title", title)
        .dialog("option", "position", {
            my: "left center",
            at: "right center",
            of: element,
            collision: "flipfit"
        })
        .data("command", command)
        .dialog("open")
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Monitor and Config page - data refresh
//

function monitorPage_refreshData() {
    $.ajax({
        url: "/data/status",
        dataType: "json",
        timeout: 3000,
        success: function (data) {

            $(".uptime").text(data.uptimeText + " since last start")

            $("#roomSetpoint").text(fmt(data.roomSetpoint, 1))
            if(data.roomTemperature > -50) {
                $("#roomTemperature").text(fmt(data.roomTemperature, 1))
                var d = fmt(data.roomError, 1)
                $("#roomError").text(d)
            }
            else {
                $("#roomTemperature").text("")
                $("#roomError").text("")
            }
            var ageClass="";
            if (data.roomTemperatureAged) ageClass="data-is-aged"
            if (data.roomTemperatureDead) ageClass="data-is-dead"
            $("#roomTemperature").removeClass("data-is-aged data-is-dead").addClass(ageClass)

            $("#roomP").text(fmt(data.roomProportionalTerm, 1))
            $("#roomI").text(fmt(data.roomIntegralTerm, 1))
            $("#roomAged").toggle(data.roomAged)
            $("#roomDead").toggle(data.roomDead)

            $("#flowSetpoint").text(fmt(data.flowSetpoint, 1))
            if(data.flowTemperature > -50) {
                $("#flowTemperature").text(fmt(data.flowTemperature, 1))
                d = fmt(data.flowError, 1, "+")
                $("#flowError").text(d)
            }
            else {
                $("#flowTemperature").text("")
                $("#flowError").text("")
            }
            var ageClass="";
            if (data.flowTemperatureAged) ageClass="data-is-aged"
            if (data.flowTemperatureDead) ageClass="data-is-dead"
            $("#flowTemperature").removeClass("data-is-aged data-is-dead").addClass(ageClass)

            $("#flowP").text(fmt(data.flowProportionalTerm, 1))
            $("#flowI").text(fmt(data.flowIntegralTerm, 1))
            $("#flowAged").toggle(data.flowAged)
            $("#flowDead").toggle(data.flowDead)

            $("#valvePosition").text(fmt(data.valvePosition, 0) + "%")
            $("#valveManualFlag").toggle(data.valveManualControl)
            if (data.sensors) {
                data.sensors.forEach(function (sensor) {
                    if (sensor.temperature > -50) {
                        $("#" + sensor.id + "-temp").text(fmt(sensor.temperature, 1))
                    }
                    else {
                        $("#" + sensor.id + "-temp").text("")
                    }
                    $("#" + sensor.id + "-crc").text(fmt(sensor.crcErrors))
                    $("#" + sensor.id + "-nr").text(fmt(sensor.noResponseErrors))
                    $("#" + sensor.id + "-oe").text(fmt(sensor.otherErrors))
                    $("#" + sensor.id + "-f").text(fmt(sensor.failures))
                    $("#" + sensor.id + "-aged").toggle(sensor.isAged)
                    $("#" + sensor.id + "-dead").toggle(sensor.isDead)
                })
            }
            if (data.zones) {
                data.zones.forEach(function (zone) {
                    $("#z" + zone.id + "-room-temp").text(fmt(zone.roomTemperature, 1))
                    $("#z" + zone.id + "-floor-temp").text(fmt(zone.foorTemperature, 1))
                    $("#z" + zone.id + "-aged").toggle(zone.isAged)
                    $("#z" + zone.id + "-dead").toggle(zone.isDead)
                    $("#z" + zone.id + "-room-off").toggle(zone.roomOff)
                    $("#z" + zone.id + "-floor-off").toggle(zone.floorOff)
                })
            }
        },
        error: function (xhr, status) {
            $(".has-data").text("")
        }
    })
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Attach functionality to controls
//
$(function () {

    // All pages: Click on the version nhumber copies the commit ID into the clipboard
    $('.navbar > .version').on('click', function () {
        var title = $(this).attr('title')
        title = title.match(/^[A-Za-z0-9]+/)[0]
        if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(title)
        } else {
            // fallback
            var tmp = $('<input>')
            $('body').append(tmp)
            tmp.val(title).select()
            document.execCommand('copy')
            tmp.remove()
        }
    })
  
    // All pages with tables: row deletion for tables that support it
    $('.delete-row').on('click', function (e) {
        e.preventDefault()
        $(this).closest('tr').remove()
    })

    // All pages with tables: row addition for tables that support it
    // Copies a row in the table marked 'template-row' to the bottom
    // of the table and calls renumberTable
    $(".add-row > td > div").on("click", function () {
        const $table = $(this).closest('table')
        const $tbody = $table.find("tbody")
        const $templateRow = $tbody.find(".template-row")
        var $newRow = $templateRow.clone(true, true)
            .removeClass("template-row")
            .show()
        $newRow.insertBefore($tbody.children("tr").last())
        renumberTable($table)
    })

    // Files page: file deletion support
    $('.delete-file').on('click', function (e) {
        e.preventDefault()
        const $row = $(this).closest('tr')
        const fileName = $row.find("a").first().attr('href')
        $.ajax({
            url: "/delete-file",
            method: "POST",
            data: { filename: fileName },
            success: function () {
                if (fileName == "/coredump.elf") $row.next('tr').remove()
                $row.remove()
            },
            error: function () {
                console.error("Failed to delete " + fileName)
            }
        })
    })

    // Pages with draggable / droppable tables: drag/drop within the table
    $('.dragDropList').sortable({
        handle: '.handle',
        helper: fixedWidthHelper,
        placeholder: 'sortable-placeholder',
        axis: 'y',
        update: function (event, ui) {
            const $table = $(this).closest("table")
            renumberTable($table)
        },
        sort: function (e, ui) {
            // get the insert row (if any)
            const $tbody = ui.placeholder.parent()
            const $last = $tbody.children('tr.add-row')
            // If the placeholder is positioned on the insert row, we move it above that
            if ($last.length && ui.placeholder.index() > $last.index()) {
                ui.placeholder.insertBefore($last)
            }
        }
    }).disableSelection()

    // Pages with draggable / droppable tables: drag/drop across tables
    $('.dragDropListAcross').sortable({
        handle: '.handle',
        helper: fixedWidthHelper,
        placeholder: 'sortable-placeholder',
        connectWith: 'tbody.dragDropListAcross',
        update: function (event, ui) {
            const $table = $(this).closest("table")
            renumberTable($table)
            var inputName = $table.find('.template-row select').first().attr('name')
            $table.find('select').attr('name', inputName)
        },
        sort: function (e, ui) {
            // get the insert row (if any)
            const $tbody = ui.placeholder.parent()
            const $last = $tbody.children('tr.add-row')
            // If placeholder is positioned on the insert row, we move it above that
            if ($last.length && ui.placeholder.index() > $last.index()) {
                ui.placeholder.insertBefore($last)
            }
        }
    }).disableSelection()

    // Manual valve controls
    $('#valveControlManualCheckbox').on('change', function (e) {
        // Ignore programmatic changes (from slider logic)
        if (!e.originalEvent) return
        valveControlManualCheckboxChanged()
    })

    $('#valveControlPositionSlider').on('change', function () {
        valveControlSliderChanged()
    })

    // Zone selection
    $('#zoneToResetSelect').on('change', function() {
        zoneToResetSelected()
    })

    // Start sequence on "Reset Valve" click
    $('#resetZoneStartButton').on('click', function () {
        startResetSequence()
    })

    // Stop and reset everything
    $('#resetZoneStopButton').on('click', function () {
        stopResetSequence()
    })

    // "Change value" dialaog
    $("#changeDialog").dialog({
        autoOpen: false,
        modal: true,
        width: 200,
        height: 140,
        closeText: "",
        buttons: [
            {
                text: "Ok",
                class: "call-to-action-button",
                click: function () {
                    sendSetValueCommand(
                        $(this).data("command"),
                        document.getElementById("newValue").value
                    )
                }
            },
            {
                text: "Cancel",
                class: "cancel-button",
                click: function () {
                    $(this).dialog("close")
                }
            }
        ]
    })

})

