import QtQuick

// Presentation only: progress remains driven by the existing dwell timer.
Item {
 id: indicator
 property real progress: 0
 property string style: "bar"
 property color color: "#00bfff"
 readonly property real amount: Math.max(0, Math.min(1, progress))
 visible: amount > 0
 clip: true
 Rectangle {
  visible: indicator.style === "expand" || (indicator.style === "collapse" && indicator.amount < 1)
  readonly property real scaleAmount: indicator.style === "expand" ? indicator.amount : 1-indicator.amount
  anchors.centerIn: parent
  width: Math.max(0,parent.width-2)*scaleAmount
  height: Math.max(0,parent.height-2)*scaleAmount
  color: "transparent"
  border.color: indicator.color
  border.width: Math.min(4,width/2,height/2)
 }
 Rectangle {
  visible: indicator.style === "bar" || indicator.style === "top-bar" || indicator.style === "dual-bar" || indicator.style === "opposing-bars" || indicator.style === "center-bars"
  x: indicator.style === "center-bars" ? (parent.width-width)/2 : indicator.style === "opposing-bars" ? parent.width-width : 0
  y: indicator.style === "top-bar" ? 0 : parent.height-height
  width: parent.width*indicator.amount; height: Math.min(5,parent.height)
  color: indicator.color
 }
 Rectangle {
  visible: indicator.style === "dual-bar" || indicator.style === "opposing-bars" || indicator.style === "center-bars"
  x: indicator.style === "center-bars" ? (parent.width-width)/2 : 0
  width: parent.width*indicator.amount; height: Math.min(5,parent.height)
  color: indicator.color
 }
 Rectangle {
  visible: indicator.style === "fill" || indicator.style === "fill-down"
  y: indicator.style === "fill-down" ? 0 : parent.height-height
  width: parent.width; height: parent.height*indicator.amount
  color: indicator.color; opacity: 0.35
 }
 Rectangle {
  visible: indicator.style === "fill-left" || indicator.style === "fill-right"
  x: indicator.style === "fill-right" ? parent.width-width : 0
  width: parent.width*indicator.amount; height: parent.height
  color: indicator.color; opacity: 0.35
 }
 Canvas {
  id: drawing
  anchors.fill: parent
  visible: ["outline","ring","pie"].indexOf(indicator.style) >= 0
  onWidthChanged: requestPaint()
  onHeightChanged: requestPaint()
  onVisibleChanged: requestPaint()
  Connections {
   target: indicator
   function onAmountChanged() { drawing.requestPaint() }
   function onStyleChanged() { drawing.requestPaint() }
   function onColorChanged() { drawing.requestPaint() }
  }
  onPaint: {
   const ctx=getContext("2d")
   ctx.reset()
   if(width<=6 || height<=6 || indicator.amount<=0) return
   ctx.strokeStyle=indicator.color;ctx.fillStyle=indicator.color;ctx.lineWidth=4
   if(indicator.style === "outline") {
    const w=width-6,h=height-6
    let remaining=2*(w+h)*indicator.amount
    ctx.beginPath();ctx.moveTo(3,3)
    const edges=[[w,0],[0,h],[-w,0],[0,-h]]
    let x=3,y=3
    for(let i=0;i<edges.length && remaining>0;i++) {
     const dx=edges[i][0],dy=edges[i][1],length=Math.abs(dx)+Math.abs(dy)
     const fraction=Math.min(remaining/length,1)
     x+=dx*fraction;y+=dy*fraction;ctx.lineTo(x,y);remaining-=length
    }
    ctx.stroke()
   } else {
    const radius=Math.min(width,height)*0.36
    const start=-Math.PI/2,end=start+2*Math.PI*indicator.amount
    ctx.beginPath()
    if(indicator.style === "pie") {ctx.globalAlpha=0.35;ctx.moveTo(width/2,height/2)}
    ctx.arc(width/2,height/2,radius,start,end)
    if(indicator.style === "pie") {ctx.closePath();ctx.fill()} else ctx.stroke()
   }
  }
 }
}
