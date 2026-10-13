//////////////////////////////////////////////////////////////////////////////////////////////
//
// General helper functions
//
function fmt(value, digits, sign) {
    if (value === null || typeof value === 'undefined') return ""
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

var zoneResetPollTimer = null
var zoneResetStatusPending = false
var zoneResetRefreshRequested = false
var zoneResetStartPending = false

function zoneToResetSelected() {
    $('#resetZoneStartButton').prop('disabled',
        zoneResetStartPending || !Number($('#zoneToResetSelect').val()))
}

function resetCommandError(xhr) {
    alert(xhr.responseJSON && xhr.responseJSON.error || 'Unable to send valve reset command')
}

function startResetSequence() {
    var zoneId = Number($('#zoneToResetSelect').val())
    if (!zoneId || zoneResetStartPending) return
    zoneResetStartPending = true
    zoneToResetSelected()
    $.ajax({
        url: '/command',
        method: 'POST',
        contentType: 'application/json',
        timeout: 3000,
        data: JSON.stringify({command: 'ValveResetStart', zoneId: zoneId}),
        success: pollValveResetStatus,
        error: resetCommandError,
        complete: function () {
            zoneResetStartPending = false
            zoneToResetSelected()
        }
    })
}

function stopResetSequence(zoneId, button) {
    $(button).prop('disabled', true)
    $.ajax({
        url: '/command',
        method: 'POST',
        contentType: 'application/json',
        timeout: 3000,
        data: JSON.stringify({command: 'ValveResetCancel', zoneId: zoneId}),
        success: pollValveResetStatus,
        error: resetCommandError,
        complete: function () {
            $(button).prop('disabled', false)
        }
    })
}

function renderValveResetStatus(zones) {
    var activeIds = new Set()
    zones.forEach(function (zone) {
        var rowId = 'zone-reset-' + zone.zoneId
        activeIds.add(rowId)
        var $row = $('#' + rowId)
        if (!$row.length) {
            $row = $($('#zoneResetRowTemplate').html()).attr('id', rowId)
            $row.attr('data-zone-id', zone.zoneId)
            $('#zoneResetRows').append($row)
        }
        $row.find('.reset-zone-name').text(zone.name)
        $row.find('.reset-zone-setpoint')
            .text(zone.setpoint === null ? '???' : fmt(zone.setpoint, 1))
            .toggleClass('zone-on', !!zone.on)
            .toggleClass('zone-off', !zone.on)
        var ticks = Math.max(0, Math.min(505, zone.ticksElapsed))
        $row.find('.reset-progress-initial .reset-progress-indicator')
            .css('left', Math.min(185, ticks) * 2 + 'px')
            .toggle(ticks <= 185)
        $row.find('.reset-progress-final .reset-progress-indicator')
            .css('left', Math.max(0, ticks - 185) + 'px')
            .toggle(ticks > 185)
        $row.find('.reset-progress').attr('aria-valuenow', ticks)
            .attr('title', ticks + ' / 505 seconds')
    })
    $('#zoneResetRows').children().each(function () {
        if (!activeIds.has(this.id)) $(this).remove()
    })
}

// One shared poll for all zones. Serialize requests so an older response cannot
// overwrite the status fetched after a start/cancel command.
function pollValveResetStatus() {
    clearTimeout(zoneResetPollTimer)
    zoneResetPollTimer = null
    if (zoneResetStatusPending) {
        zoneResetRefreshRequested = true
        return
    }
    zoneResetStatusPending = true
    var keepPolling = true // Retry transient failures without discarding rows.
    $.ajax({
        url: '/command',
        method: 'POST',
        contentType: 'application/json',
        dataType: 'json',
        timeout: 3000,
        data: JSON.stringify({command: 'ValveResetStatus'}),
        success: function (zones) {
            if (!Array.isArray(zones)) return
            renderValveResetStatus(zones)
            keepPolling = zones.length > 0
        },
        complete: function () {
            zoneResetStatusPending = false
            if (zoneResetRefreshRequested) {
                zoneResetRefreshRequested = false
                pollValveResetStatus()
            } else if (keepPolling) {
                zoneResetPollTimer = setTimeout(pollValveResetStatus, 1000)
            }
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

    var value = parseInt($('#valveControlPositionSlider').val(), 10) || 0
    $('#valveControlPositionText').text(value)

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
// Config page - Heating Parameters field visibility toggle
//

function toggleHeatingControlFields() {
    const mode = $('select[name="mode"]').val();

    if (mode === "0") { // Room Temperature Control
        $('input[name^="room-"]').closest('tr').show()
        $('input[name^="wc-"]').closest('tr').hide()
        $('input[name^="hybrid-"]').closest('tr').hide()
    }
    else if (mode === "1") { // Weather Compensation
        $('input[name^="room-"]').closest('tr').hide()
        $('input[name^="wc-"]').closest('tr').show()
        $('input[name^="hybrid-"]').closest('tr').hide()
    }
    else if (mode === "2") { // Hybrid
        $('input[name^="room-"]').closest('tr').show()
        $('input[name^="wc-"]').closest('tr').show()
        $('input[name^="hybrid-"]').closest('tr').show()
    }
    
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
            if(data.roomTemperature !== null) {
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

            $("#flowP").text(fmt(data.flowProportionalTerm, 1))
            $("#flowI").text(fmt(data.flowIntegralTerm, 1))
            $("#roomAged").toggle(data.roomAged)
            $("#roomDead").toggle(data.roomDead)

            if (data.flowSetpoint !== null) {
                $("#flowSetpoint").text(fmt(data.flowSetpoint, 1))
            }
            else {
                $("#flowSetpoint").text("OFF")
            }
            if (data.flowSetpoint !== null && data.flowRaw !== null) {
                $("#flowRaw").text(fmt(data.flowRaw, 1))
                $("#flowRaw").show();
            }
            else {
                $("#flowRaw").text("")
                $("#flowRaw").hide();
            }
            if (data.flowSetpoint !== null && data.flowTweak !== null) {
                $("#flowTweak").text(fmt(data.flowTweak, 1, "+"))
                $("#flowTweak").show();
            }
            else {
                $("#flowTweak").text("")
                $("#flowTweak").hide();
            }
            
            if(data.flowTemperature !== null) {
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

            $("#valveP").text(fmt(data.valveProportionalTerm, 1))
            $("#valveI").text(fmt(data.valveIntegralTerm, 1))
            $("#flowAged").toggle(data.flowAged)
            $("#flowDead").toggle(data.flowDead)

            if (data.valvePosition !== null){
                $("#valvePosition").text(fmt(data.valvePosition, 0) + "%")
            }
            else {
                $("#valvePosition").text("")
            }
            $("#valveManualFlag").toggle(data.valveManualControl)
            if (data.valveOpening > 0) {
                $("#valvePosition").text("Waiting: " + data.valveOpening + "s").show()
            }
            else {
                $("#valveOpening").hide()
            }
            if (data.sensors) {
                data.sensors.forEach(function (sensor) {
                    if (sensor.temperature !== null) {
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

    // Toggling showing the extra columns in tables which can show additional information
    $('table.hide-extras th.toggle-extras-button').on('click', function () {
        const $table = $(this).closest('table')
        if ($table.hasClass('hide-extras')) {
            $table.removeClass('hide-extras')
            $table.addClass('show-extras')
        } 
        else {
            $table.removeClass('show-extras')
            $table.addClass('hide-extras')
        }
    });
        
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

    // Control mode (Weather Compensation toggle)
    $('select[name="mode"]').on('change', function() {
        toggleHeatingControlFields()
    })

    // Start sequence on "Reset Valve" click
    $('#resetZoneStartButton').on('click', function () {
        startResetSequence()
    })

    $('#zoneResetRows').on('click', '.reset-zone-cancel', function () {
        stopResetSequence(Number($(this).closest('tr').attr('data-zone-id')), this)
    })

    // Discover controller-managed resets when opening or reloading this page.
    if ($('#zoneResetRows').length) {
        zoneToResetSelected()
        pollValveResetStatus()
    }

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

