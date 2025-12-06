// General helper functions
function fmt(value, digits) {
  var x = Number(value).toFixed(digits)
  if (x == "NaN") x = value
  return x
}

// Drag - drop support
function fixedWidthHelper(e, tr) {
  const $orig = tr.children();
  const $helper = tr.clone();
  $helper.children().each(function (i) {
    $(this).width($orig.eq(i).width());
  });
  return $helper;
}

function renumberTable($table) {
  $table.find("tbody tr").not(".template-row").each(function (i) {
    const $first = $(this).children("td").eq(0);
    if (!$first.hasClass("seq")) return;  // skip this row
    $first.text(i+1);
  });
}

function sendCommand(payload) {
  $.ajax({
    url: '/command',
    method: 'POST',
    contentType: 'application/json',
    data: JSON.stringify(payload),
    success: function (response) {
      if (response && response.reload === true) {
        location.reload();
      }
    }
  });
}

$(function () {

  /* Click on the version nhumber copies the commit ID into the clipboard */
  $('.navbar > .version').on('click', function () {
    var title = $(this).attr('title')
    title = title.match(/^[A-Za-z0-9]+/)[0]
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(title);
    } else {
        // fallback
        var tmp = $('<input>');
        $('body').append(tmp);
        tmp.val(title).select();
        document.execCommand('copy');
        tmp.remove();
    }
  });
  
  /* row deletion and addition for tables that support it*/
  $('.delete-row').on('click', function (e) {
    e.preventDefault();
    $(this).closest('tr').remove();
  });

  $(".add-row > td > div").on("click", function () {
    const $table = $(this).closest('table')
    const $tbody = $table.find("tbody")
    const $templateRow = $tbody.find(".template-row");
    var $newRow = $templateRow.clone(true, true)
      .removeClass("template-row")
      .show();
    $newRow.insertBefore($tbody.children("tr").last());
    renumberTable($table);
  });

  /* row deletion and addition for tables that support it*/
  $('.delete-file').on('click', function (e) {
    e.preventDefault();
    const $row = $(this).closest('tr');
    const fileName = $row.find("a").first().text();
    $.ajax({
        url: "/delete-file",
        method: "POST",
        data: { filename: fileName },
        success: function () {
            $row.remove();
        },
        error: function () {
            console.error("Failed to delete " + fileName);
        }
    });
  });

  $('.dragDropList').sortable({
    handle: '.handle',
    helper: fixedWidthHelper,
    placeholder: 'sortable-placeholder',
    axis: 'y',
    update: function (event, ui) {
      const $table = $(this).closest("table");
      renumberTable($table);
    },
    sort: function (e, ui) {
      // get the insert row (if any)
      const $tbody = ui.placeholder.parent()
      const $last = $tbody.children('tr.add-row');
      // If the placeholder is positioned on the insert row, we move it above that
      if ($last.length && ui.placeholder.index() > $last.index()) {
        ui.placeholder.insertBefore($last);
      }
    }
  }).disableSelection();


  $('.dragDropListAcross').sortable({
    handle: '.handle',
    helper: fixedWidthHelper,
    placeholder: 'sortable-placeholder',
    connectWith: 'tbody.dragDropListAcross',
    update: function (event, ui) {
      const $table = $(this).closest("table");
      renumberTable($table);
      var inputName = $table.find('.template-row select').first().attr('name')
      $table.find('select').attr('name', inputName);
    },
    sort: function (e, ui) {
      // get the insert row (if any)
      const $tbody = ui.placeholder.parent()
      const $last = $tbody.children('tr.add-row');
      // If placeholder is positioned on the insert row, we move it above that
      if ($last.length && ui.placeholder.index() > $last.index()) {
        ui.placeholder.insertBefore($last);
      }
    }
  }).disableSelection();

  var $slider = $('#valveControlPositionSlider');
  var $sliderValue = $('#valveControlPositionValue');
  var $checkbox = $('#valveControlManual');

  $checkbox.on('change', function (e) {
      // Ignore programmatic changes (from slider logic)
      if (!e.originalEvent) return;

      var isChecked = $checkbox.is(':checked');
      var automatic = !isChecked; // true if unchecked, false if checked

      var value = parseInt($slider.val(), 10) || 0;
      $sliderValue.text(value);

      sendCommand({
        command: 'SetValvePosition',
        parameters: {
          automatic: automatic,
          position: value
        }
      });
    });

    $('#valveControlPositionSlider').on('change', function () {

      var value = parseInt($slider.val(), 10) || 0;
      $sliderValue.text(value);

      // Ensure checkbox is checked; do not trigger its AJAX handler
      if (!$checkbox.is(':checked')) {
        $checkbox.prop('checked', true).triggerHandler('change');
      }

      sendCommand({
        command: 'SetValvePosition',
        parameters: {
          automatic: false,
          position: value
        }
      });
    });

});

// Data refresh - used by both monitor and config pages
function monitorPage_refreshData() {
  $.ajax({
    url: "/data/status",
    dataType: "json",
    timeout: 3000,
    success: function (data) {
      $("#roomSetpoint")   .text(fmt(data.roomSetpoint, 1));
      $("#roomTemperature").text(fmt(data.roomTemperature, 1));
      $("#roomD")          .text(fmt(data.roomProportionalTerm, 1));
      $("#roomI")          .text(fmt(data.roomIntegralTerm, 1));

      $("#flowSetpoint")   .text(fmt(data.flowSetpoint, 1));
      $("#flowTemperature").text(fmt(data.flowTemperature, 1));
      $("#flowD")          .text(fmt(data.flowProportionalTerm, 1));
      $("#flowI")          .text(fmt(data.flowIntegralTerm, 1));

      var d = fmt(data.roomError, 1);
      if (fmt != '0.0' && data.roomError > 0) d = '+' + d;
      $("#roomError") .text(d);

      d = fmt(data.flowError, 1);
      if (fmt != '0.0' && data.flowError > 0) d = '+' + d;
      $("#flowError").text(d);

      $("#valvePosition")  .text(fmt(data.valvePosition, 0) + "%");
      $("#valveManualFlag").toggle(data.valveManualControl)
      if (data.sensors) {
        data.sensors.forEach(function (sensor) {
          $("#" + sensor.id + "-temp").text(fmt(sensor.temperature, 1))
          $("#" + sensor.id + "-crc").text(fmt(sensor.crcErrors))
          $("#" + sensor.id + "-nr").text(fmt(sensor.noResponseErrors))
          $("#" + sensor.id + "-oe").text(fmt(sensor.otherErrors))
          $("#" + sensor.id + "-f").text(fmt(sensor.failures))
        })
      }
      if (data.zones) {
        data.zones.forEach(function (zone) {
          $("#z" + zone.id + "-room-temp").text(fmt(zone.roomTemperature, 1))
          $("#z" + zone.id + "-floor-temp").text(fmt(zone.foorTemperature, 1))
          if (zone.roomOff) {
            $("#z" + zone.id + "-room-temp").closest("td").addClass("off");
          }
          else {
            $("#z" + zone.id + "-room-temp").closest("td").removeClass("off");
          }
          if (zone.floorOff) {
            $("#z" + zone.id + "-floor-temp").closest("td").addClass("off");
          }
          else {
            $("#z" + zone.id + "-floor-temp").closest("td").removeClass("off");
          }
        })
      }
    },
    error: function (xhr, status)  {
      $(".has-data").text("");
    }
  });
}
