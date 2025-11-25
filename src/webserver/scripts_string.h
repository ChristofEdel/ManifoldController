#pragma once
#include <pgmspace.h>

const char SCRIPTS_JS_STRING[] PROGMEM = R"RAW_STRING(
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
$(function () {
  $('.delete-row').on('click', function (e) {
    e.preventDefault();
    $(this).closest('tr').remove();
  });
  $('.sensorList').sortable({
    handle: '.handle',
    helper: fixedWidthHelper,
    placeholder: 'sortable-placeholder',
    axis: 'y'
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
)RAW_STRING";