const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang='en'>
<head>
<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />
<!--meta http-equiv="refresh" content="3"-->
<meta name='mobile-web-app-capable' content='yes' />
<meta name='viewport' content='width=device-width' />
<title>Kitchen LED (RGB LED Color) Controller</title>
</head>
<body style="background:@@color@@;">

<center>
<p>Random #1 <a href="?Random=On"><button>ON</button></a>&nbsp;<a href="?Random=Off"><button>OFF</button></a></p>
<div class="card">
  <div class="header">
    Kitchen LED (RGB LED Color) Controller
  </div>
  <form method="post" action="/form">
    <table>
    <tr>
      <td>Color:</td>
      <td><input type="color" name="color" value="@@color@@"></td>
      <td>
        <button type="submit" name="state" value="stop">SET</button>
      </td>
    </tr>
  </table>
  </form>
</div>


<p>Brightness</p>
<left><input type="range" min="0" max="255" value=Brightness step="5" onchange="showSpeed(this.value)" size="80" align="left" > </input></left>
<span id="range"></span>
<script type="text/javascript">function showSpeed(newValue){document.getElementById("range").innerHTML=newValue;var req = new XMLHttpRequest();req.open('GET', '?' + 'Brightness=' + newValue, true);req.send();}</script>


<br>
<a href="/settings">Settings</a><br>
</center>
</body>
</html>
)=====";
