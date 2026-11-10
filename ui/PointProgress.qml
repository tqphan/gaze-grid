import QtQuick

Canvas {
 id: indicator
 property real progress: 0
 property string style: "ring"
 property color color: "#00bfff"
 property real thickness: 5
 onProgressChanged: requestPaint()
 onStyleChanged: requestPaint()
 onColorChanged: requestPaint()
 onThicknessChanged: requestPaint()
 onWidthChanged: requestPaint()
 onHeightChanged: requestPaint()
 onVisibleChanged: requestPaint()
 onPaint: {
  const ctx=getContext("2d")
  ctx.reset()
  const amount=Math.max(0,Math.min(1,progress))
  const radius=Math.max(0,(Math.min(width,height)-thickness)/2-2.5)
  if(radius<=0)return
  ctx.strokeStyle=color;ctx.fillStyle=color
  ctx.lineWidth=Math.min(3,thickness);ctx.globalAlpha=0.35
  ctx.beginPath();ctx.arc(width/2,height/2,radius,0,2*Math.PI);ctx.stroke()
  ctx.globalAlpha=1;ctx.lineWidth=thickness
  ctx.beginPath()
  if(style==="pie") {
   ctx.moveTo(width/2,height/2)
   ctx.arc(width/2,height/2,radius,-Math.PI/2,-Math.PI/2+2*Math.PI*amount)
   ctx.closePath();ctx.fill()
  } else if(style==="expand") {
   if(amount>0) {
    ctx.arc(width/2,height/2,radius*amount,0,2*Math.PI);ctx.stroke()
   }
  } else if(style==="shrink") {
   if(amount<1) {
    ctx.arc(width/2,height/2,radius*(1-amount),0,2*Math.PI);ctx.stroke()
   }
  } else {
   ctx.arc(width/2,height/2,radius,-Math.PI/2,-Math.PI/2+2*Math.PI*amount);ctx.stroke()
  }
 }
}
