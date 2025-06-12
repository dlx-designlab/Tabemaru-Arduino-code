function doGet(e) {
  var sheet = SpreadsheetApp.openById("1B1K7XADv-me4h64Jfb9GiWjOlCR1Hutsnw6i0eiHDfs").getSheetByName("Sheet1");
  var temp1 = e.parameter.temp1 || "N/A"; // Valeur par défaut si absent
  var hum1 = e.parameter.hum1 || "N/A";   // Valeur par défaut si absent
  var temp2 = e.parameter.temp2 || "N/A"; // Valeur par défaut si absent
  var hum2 = e.parameter.hum2 || "N/A";   // Valeur par défaut si absent
  var moist = e.parameter.moist || "N/A";
  var time = new Date();
  sheet.appendRow([time, temp1, hum1, temp2, hum2,moist]);
  return ContentService.createTextOutput("OK")
    .setMimeType(ContentService.MimeType.TEXT);
}