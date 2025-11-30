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

$(function () {
  
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
});

// Data refresh - used by both monitor and config pages
function monitorPage_refreshData() {
  $.ajax({
    url: "/data/status",
    dataType: "json",
    timeout: 3000,
    success: function (data) {
      $("#flowSetpoint").text(fmt(data.flowSetpoint, 1));
      $("#valvePosition").text(fmt(data.valvePosition, 0) + "%");
      data.sensors.forEach(function (sensor) {
        $("#" + sensor.id + "-temp").text(fmt(sensor.temperature, 1))
        $("#" + sensor.id + "-crc").text(fmt(sensor.crcErrors))
        $("#" + sensor.id + "-nr").text(fmt(sensor.noResponseErrors))
        $("#" + sensor.id + "-oe").text(fmt(sensor.otherErrors))
        $("#" + sensor.id + "-f").text(fmt(sensor.failures))
      })
    },
    error: function (xhr, status)  {
      $(".has-data").text("");
    }
  });
}
