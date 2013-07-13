
var margin = {top: 20, right: 20, bottom: 30, left: 50},
    width = 200 - margin.left - margin.right,
    height = 160 - margin.top - margin.bottom;

var svg = d3.select("body").append("svg")
.attr("class", "chart")
.attr("width", width + margin.left + margin.right)
.attr("height", height + margin.top + margin.bottom)
.append("g")
.attr("transform", "translate(" + margin.left + "," + margin.top + ")");

var parseDate = d3.time.format("%Y-%M-%d %H:%M:%S").parse,
formatPercent = d3.format(".0%");

var y = d3.scale.linear()
	.range([height, 0]);
var x = d3.scale.ordinal()
	.rangeBands([0, width]);
function updateD3Area(specificFile) {
	d3.tsv("data_tsv.php?file="+specificFile, function(error, data) {
		y.domain([0, d3.max(data, function(d) { return d.Totaltime; })]);
		//y.domain(d3.extent(data, function(d) { return d.date; }));
		var barwid = width/data.length;
		svg.selectAll("rect")
		.data(data)
		.enter().append("rect")
		.attr("x", function(d, i) { return i * barwid; })
		.attr("y", function(d) {return y(d.Totaltime);})
		.attr("height", function(d) {return height -  y(d.Totaltime);})
		.attr("width",barwid);
	});
}
