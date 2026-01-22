#pragma once
#include <pgmspace.h>

const char STYLES_CSS_STRING[] PROGMEM = R"RAW_STRING(
/**********************************************************************************
** Colours
*/

:root {
  --bg-global:           white;
  --color-dark-grey:     #505050;
  --border-wide:         3px solid var(--color-dark-grey);
  --border-narrow:       1px solid var(--color-dark-grey);
  --color-text-active:   black;
  --color-text-inactive: #707070;
  --bg-active:           #dddddd;
  --bg-inactive:         var(--bg-global);
  --bg-titles:           #e0e0e0;
  --bg-even:             #f4f4f4;
  --bg-odd:              var(--bg-global);
  --bg-warning:          yellow;
  --bg-error:            red;
  --bg-inverted:         black;
  --text-inverted:       white;
  --text-warning:        black;
  --text-error:          white;
  --color-red:           red;
  --color-green-text:    darkgreen;
  --color-green-bg:      lightgreen;
  --color-call-to-action:orange;
}


/**********************************************************************************
** Page: center and limit to 880px width
*/

.page-div {
    margin-left: auto;
    margin-right: auto;
    max-width: 800px;
}


/**********************************************************************************
** Navigation bar
*/

.navbar {
    display: flex;
    flex-direction: row;
    gap: 5px;                 /* 5px between boxes */
}

.navbar > .navbox {
    width: 5em;
    text-align: center;
    font-weight: bold;
    background: var(--bg-inactive);
    border: var(--border-wide);
    border-bottom: none;
    border-radius: 8px 8px 0 0;   /* rounded top corners only */
    padding: 5px;
}
.navbar > .version {
    margin-left: auto;
    align-self: flex-end;
    display: flex;
    align-items: flex-end;
    font-size: small;
    color: var(--bg-active);
}

.navbar > .navbox > a {
    color: var(--color-text-inactive);
    text-decoration: none;
    display: block;           /* full box is clickable */
}

.navbar > .navbox.selected {
    background: var(--bg-active);
}
.navbar > .navbox.selected a {
    color: var(--color-text-active);
}

.navbar-border {
    border-top: var(--border-wide);
    margin-bottom: 5px;
}

.bottom-bar > a {
    color: var(--color-text-inactive);
    text-decoration: none;
    font-size: small;
    display: block;           /* full box is clickable */
}
.bottom-bar > a:visited {
    color: var(--color-text-inactive);
}

.tooltip {
    position: relative;
    display: inline-block;
}

.tooltip:hover .tip {
    visibility: visible;
    opacity: 1;
}

.tip {
    visibility: hidden;
    opacity: 0;
    transition: opacity .2s;

    position: absolute;
    z-index: 999;

    background: #333;
    color: #fff;
    padding: 6px 10px;
    border-radius: 4px;

    white-space: pre-line;  /* enables \n */
    width: max-content;     /* grow to content width */
    max-width: none;        /* ignore parent max-width */
    display: block;

}


/**************************************************************************************
**
** Dialog Pop-Ups
*/

/* Main dialog */
.ui-dialog {
    border: var(--border-wide);
    background: var(--bg-global)
}

/* Header / title bar */
.ui-dialog .ui-dialog-titlebar {
    background: var(--bg-titles);
    border: none;
    border-bottom: var(--border-wide);
    padding: 3px;
    font-weight: bold;
}

/* Close (X) button */
.ui-dialog .ui-dialog-titlebar-close {
    border: none;
    background: transparent;
}

.ui-dialog .ui-dialog-titlebar-close {
    width: 21px;
    cursor: pointer;
    text-align: center;
    color: var(--color-red);
    position: absolute;
    right: 4px;
    top: 3px;
    padding: 0
}

.ui-dialog .ui-dialog-titlebar-close::before {
    content: "\2716";
}

/* Content */
.ui-dialog .ui-dialog-content {
    padding: 16px;
}

/* Button bar */
.ui-dialog .ui-dialog-buttonpane {
    background: #fff;
    border-top: 3px solid #555;
    padding: 10px;
}

/* Button container */
.ui-dialog .ui-dialog-buttonset {
    display: flex;
    gap: 10px;
}

/* All dialog buttons */
.ui-dialog .ui-dialog-buttonpane button {
    flex: 1;                 /* equal width */
    margin: 0;
}


/**********************************************************************************
** Straight table layout for table with titles on the top
*/

table.list {
    margin: 0 auto;
    font-size: 12pt;
    border-collapse: collapse;
    border-bottom: var(--border-narrow);
}

table.list > thead > tr > th,
table.list > tbody > tr > th {
    background: var(--bg-titles);
    border-top: var(--border-wide);
    border-bottom: var(--border-narrow);
}

table.list > tbody > tr > th:first-child,
table.list > thead > tr > th:first-child {
    text-align: left;
}

table.list > thead > tr > th,
table.list > thead > tr > td,
table.list > tbody > tr > th,
table.list > tbody > tr > td {
    padding: 5px;
}

table.list > tbody > tr:nth-child(even)
{
    background: var(--bg-even);
}
table.list > tbody > tr:nth-child(odd)
{
    background: var(--bg-odd);
}

/**********************************************************************
** Directory
*/

table.directory > tbody > tr > td:nth-child(1){ /* file name */
    width:210px;
}
table.directory > tbody > tr > td:nth-child(2){ /* size */
    width:70px;
}
table.directory > tbody > tr > td:nth-child(3){ /* date */
    width:145px;
    text-align: center;
}

/**********************************************************************************
** Tables with field names in column 1 and fields in column 2
*/

table.field-table > thead > tr > th {   /* if it has a head, thin line at bottom of head */
    border-bottom: var(--border-wide);
}

table.field-table > tbody > tr > th {   /* headers in each row */
    text-align: left;
    font-weight: normal;
    padding-left: 10px;
    padding-right: 5px; 
    padding-top: 5px;
    padding-bottom: 5px;
}

table.field-table input {               /* input field style within table */
    background: transparent;
    border: none;
    border-bottom: var(--border-narrow);
}

.block-layout .content table.field-table { /* Special case: eliminate parent padding if in block-layoud */
    margin: -5px;
    margin-left: -10px;
    border-collapse: collapse;
}


/**********************************************************************************
** Table for lists of sensors or manifolds when monitoring
*/

table.monitor-table {
    border-collapse: collapse;
}

table.monitor-table > tbody > tr > th:first-child,
table.monitor-table > thead > tr:first-child > th:first-child {
    text-align: left;
}

table.monitor-table > thead > tr > th {
    border-bottom: var(--border-narrow);
}

table.monitor-table > tbody  > tr > th {
    font-weight: normal;
    min-width: 120px;
}

table.monitor-table > tbody > tr > th {
    text-align: left;
}
table.monitor-table > tbody > tr > td {
    text-align: center;
    width: 50px;
}

table.monitor-table > thead > tr > th {
    padding: 5px;
}
table.monitor-table > tbody > tr > th,
table.monitor-table > tbody > tr > td {
    padding: 5px;
}

table.monitor-table > tbody > tr:nth-child(even)
{
    background: var(--bg-even);
}
table.monitor-table > tbody > tr:nth-child(odd)
{
    background: var(--bg-odd);
}

.block-layout .content table.monitor-table { /* Special case: eliminate parent padding if in block-layoud */
    margin: -8px;
    margin-left: -5px;
}

table.monitor-table > tbody > tr > td.on {
    background-color: rgb(134, 190, 134);
}
table.monitor-table > tbody > tr > td.off {
    background-color: red;
    color: white;
    font-weight: bold;
}
table.monitor-table > tbody > tr > th.active {
    font-weight: bold;
}

table.field-table > tbody > tr > td.data-is-off,
table.monitor-table > tbody > tr > td.data-is-off {
    background-color: var(--bg-inverted);
    color: var(--text-inverted);
    font-weight: bold;
}
table.field-table > tbody > tr > td.data-is-aged,
table.monitor-table > tbody > tr > td.data-is-aged {
    background-color: var(--bg-warning);
    color: var(--text-warning);
    font-weight: bold;
}
table.field-table > tbody > tr > td.data-is-dead,
table.monitor-table > tbody > tr > td.data-is-dead {
    background-color: var(--bg-error) ;
    color: var(--text-error) ;
    font-weight: bold;
}

/**********************************************************************************
** Hide/show extra columns
*/

table.control-table.hide-extras > tbody > tr > td:nth-child(n+5),
table.control-table.hide-extras > thead > tr > th:nth-child(n+5)
{
    display: none;
}

table.monitor-table.hide-extras > tbody > tr > td:nth-child(n+3),
table.monitor-table.hide-extras > thead > tr:nth-child(1) > th:nth-child(n+3),
table.monitor-table.hide-extras > thead > tr:nth-child(2)
{
    display: none;
}

table.monitor-table.hide-extras {
    margin-top: -3px !important; /* instead of -8 - an extra 5 pixels */
}

table.hide-extras > thead > tr > th.toggle-extras-button,
table.show-extras > thead > tr > th.toggle-extras-button {
    display: table-cell !important;
    border-bottom: none;
    color: var(--color-text-inactive);
    cursor: pointer;
    text-align: left; padding-left: 0px; vertical-align: bottom;
}
table.hide-extras > thead > tr > th.toggle-extras-button::before {
    content: "\23F5";
}
table.show-extras > thead > tr > th.toggle-extras-button::before {
    content: "\23F4";
}


/**********************************************************************************
** re-usable styles for all tables
*/

th.gap-right, td.gap-right {
    border-right: 5px solid var(--bg-global);
}

table.tight > thead > tr >  th,
table.tight > thead > tr >  td,
table.tight > tbody > tr >  th,
table.tight > tbody > tr >  td,
tr.tight > th,
tr.tight > td {
    padding-top: 0px !important;
    padding-bottom: 0px!important;
}

td.right {
    text-align: right;
}

input.num-3em {
    text-align: right;
    width: 3em;
}
input.num-4em {
    text-align: right;
    width: 4em;
}

table.center-all-td > thead > tr > td,
table.center-all-td > tbody > tr > td {
    text-align: center;
}

/**********************************************************************************
** Representation of switches
*/

div.switch-state {
    box-sizing: border-box;
    text-wrap: nowrap;
    text-align: center;
    width: 90px;
}
div.switch-state.on {
    background: var(--color-green-bg);
}
div.switch-state.off {
    background: var(--bg-active);
}


/**********************************************************************************
** Insert / Delete / Drag-Drop styling
*/

.template-row {
    display: none;
}

.delete-row, 
.delete-file 
{
    width: 21px;
    cursor: pointer;
    text-align: center;
    color: var(--color-red);
}

.delete-file::before,
.delete-row::before,
.ui-dialog .ui-dialog-titlebar-close::before
{
    content: "\2716";
}

.delete-header {
    width: 21px;
}
.add-row > td {
    padding-top: 0;
    text-align: right;
}
.add-row > td > div {
    width: 21px;
    display: inline-block;
    cursor: pointer;
    text-align: center;
    color: var(--color-green);
}
.add-row > td > div::before {
    content: "\271A";
}
.add-row > td {
    border-top: var(--border-narrow);
    padding:0
}

.handle {
    cursor: move;
    text-align: center;
}

.sortable-placeholder {
    background: var(--bg-even);
    visibility: visible !important;
}

/**********************************************************************************
// Sliders
*/

td.slider {
    display:flex; align-items:center; gap:8px;
}

.manual-control {
    background-color: red;
    color: white;
    font-weight: bold;
    padding-left: 5px;
    padding-right: 5px;
}

.sensor-scan {
    font-size: small;
    color: var(--color-text-inactive);
    padding-right: 5px;
    padding-bottom: 2px;
    text-decoration: none;
    position: absolute; bottom: 0; right: 0;
}
.sensor-scan :visited {
    color: var(--color-text-inactive);
}


/**********************************************************************************
** Buttons
*/

button {
    border: var(--border-wide);
    display:block;
    font-weight: bold;
    padding: 5px;
}

button.call-to-action-button {
    background-color: var(--color-call-to-action);
}

button.cancel-button {
    background-color: var(--bg-titles);
}

button.call-to-action-button:hover,
button.cancel-button:hover{
    filter: brightness(0.9);
}

button.save-button {
    margin: 10px auto 0px auto;
}


/* "Block Layout" with bold titles on the left and content on the right;
** adapts to titles on top on narrow screens (mobile)
*/

/* bottom border for the whole block */
.block-layout {
    border-bottom: var(--border-wide);
}

.block-layout * {
    box-sizing: border-box;
}

.block-layout .block {
    display: flex;
}

.block-layout .title {
    width: 180px;
    flex-shrink: 0;
    padding: 8px;
    background: var(--bg-titles);
    font-weight: bold;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    border-top: var(--border-wide);
    position: relative;
}

.block-layout .content {
    flex: 1;
    padding: 8px;
    border-top: var(--border-wide);
}

.flex-wrap {
    display: flex;
    flex-wrap: wrap;
    gap: 30px;
    align-items: flex-start;
}
.flex-wrap table {
    flex: 0 0 auto;
}

/* collapsed (narrow) view */
@media (max-width: 600px) {
    .block-layout .block {
        flex-direction: column;
    }

    .block-layout .title {
        width: 100%;
        border-top: var(--border-wide);
        /* top of the pair */
    }

    .block-layout .content {
        width: 100%;
        border-top: var(--border-narrow);
        /* only 1px between title and content */
    }
}

/**********************************************************************
** Reset Valve function on config page
*/

#resetZoneSetpoint {
    width: 2.5em;
    display: block;
    text-align: center;
    height: 20px
}
.zone-on {
    background: var(--color-green-bg);
    display: inline;
}
.zone-off {
    background: var(--bg-active);
    display: inline;
    padding-left: 5px;
    padding-right: 5px;
}
.reset-progress-bar {
    display: flex;
    flex-wrap: nowrap;
    padding: 0;
    margin: 0;
    position: relative;
    font-size: small
}
.reset-progress-bar > div {
    flex: 0 0 auto;         /* keep fixed width from inline style */
    display: inline-block;
    text-align: center;
    margin:0px;
}
.reset-progress-bar > div.on {
    background-color: var(--color-green-bg);
}
.reset-progress-bar > div.off {
    background-color: var(--bg-active);
}
#resetProgressIndicator {
    height: 100%;
    width: 2px;
    background-color: black;
    position: absolute;
    left: 19px;
}
)RAW_STRING";