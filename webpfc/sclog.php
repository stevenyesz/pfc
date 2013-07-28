
<?
require './config.php';
require_once 'convert.php';

$sclogDir = Pfc_Config::storageDir()."/"."sclog";
$dataFile = $_GET['dataFile'];
$dataFile = str_replace('pfc.out', 'pfc.sclog', $dataFile);
$realPaht = $sclogDir."/".$dataFile;
$rawData = file_get_contents($realPaht);
$barData = getResultData($rawData, 600);
?>
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
	"http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
<link rel="stylesheet" type="text/css" href="styles/d3drawarea.css">
<script src="js/d3.v3.min.js"></script>

</head>
<body>
<script type="text/javascript" charset="utf-8">
var margin = {top: 20, right: 20, bottom: 30, left: 50},
width = 800 - margin.left - margin.right,
height = 8000 - margin.top - margin.bottom;

var svg = d3.select("body").append("svg")
.attr("class", "chart")
.attr("width", width + margin.left + margin.right)
.attr("height", height + margin.top + margin.bottom)
.append("g")
.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

var data = <?php echo $barData;?>;
svg.selectAll("rect")
.data(data)
.enter().append("rect")
.attr("x", function(d) { return d.left; })
.attr("y", function(d) {return d.packingStart;})
.attr("height", function(d) {return d.duration;})
.attr("width",function(d) {return d.width;});
/*
svg.selectAll("text")
.data(data)
.enter().append("text")
.attr("x", function(d) { return d.left+d.width/2; })
.attr("y", function(d) {return d.packingStart+d.duration/2;})
.attr("dx", -3) // padding-right 右边距
.attr("dy", ".35em") // vertical-align: middle 标签垂直居中
.attr("text-anchor", "end") // text-align: right 文字水平居右
.text(function(d) {return '#'+ d.orderId;});
*/
</script>
</body>

</html>