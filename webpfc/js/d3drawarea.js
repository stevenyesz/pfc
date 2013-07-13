
var margin = {top: 20, right: 20, bottom: 30, left: 50},
    width = 200 - margin.left - margin.right,
    height = 160 - margin.top - margin.bottom;

var svg = d3.select("body").append("svg")
.attr("class", "chart")
.attr("width", width + margin.left + margin.right)
.attr("height", height + margin.top + margin.bottom)
.append("g")
.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

function updateD3Area(specificFile) {
	d3.tsv("data_tsv.php?file="+specificFile, function(error, data) {
		var tmax = d3.max(data, function(d) {return parseInt(d.Totaltime);});
		var yh = height/tmax;
		var barwid = width/data.length;
		svg.selectAll("rect")
		.data(data)
		.enter().append("rect")
		.attr("x", function(d, i) { return i * barwid; })
		.attr("y", function(d) {return height - yh*d.Totaltime;})
		.attr("height", function(d) {return  (yh * d.Totaltime);})
		.attr("width",barwid);
	});
}
