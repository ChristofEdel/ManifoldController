#pragma once
#include <pgmspace.h>

const char SCRIPTS_JS_STRING[] PROGMEM = R"RAW_STRING(
  function fixedWidthHelper(e, tr) {
    const $orig = tr.children();
    const $helper = tr.clone();
    $helper.children().each(function(i) {
      $(this).width($orig.eq(i).width());
    });
    return $helper;
  }
  $(function() {
    $('.delete-row').on('click', function(e) {
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
)RAW_STRING";