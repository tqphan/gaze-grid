import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.desktop as Desktop
import org.kde.kirigami as Kirigami
Window {
 id: root
 objectName: "keyboardWindow"
 width: 1100; height: 370; visible: false
 title: "gaze-grid"
 // Keep the native surface translucent for its entire lifetime.
 color:"transparent"
 flags: Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus | Qt.WindowStaysOnTopHint
 property string hovered: ""
 property bool canvasDragging:false
 property point canvasPoint:Qt.point(0,0)
 // Keep coordinates current before entering edit mode as well.
 HoverHandler {id:canvasHover}
 property var dwellStyleIds: ["bar","top-bar","fill","fill-down","fill-left","fill-right","outline","ring","pie","collapse","expand"]
 property var dwellStyleNames: ["Bottom Bar","Top Bar","Fill Up","Fill Down","Fill Right","Fill Left","Outline Trace","Ring Progress","Pie Progress","Shrink Inward","Grow Outward"]
 property real progress: 0
 readonly property var transitionIds:["instant","fade","lift","slide","zoom","rows"]
 readonly property var transitionNames:["Instant Switch","Soft Fade","Gentle Lift","Directional Slide","Subtle Zoom","Row Reveal"]
 property string displayedLayout:""
 property string activeTransition:"instant"
 property real revealProgress:1
 readonly property bool transitioning:layoutReveal.running
 property int slideDirection:1
 function revealLayout() {
  if(displayedLayout===controller.layoutName)return
  const previous=displayedLayout
  displayedLayout=controller.layoutName
  layoutReveal.stop()
  revealProgress=1
  activeTransition=controller.keyboardTransition
  if(!previous||!root.visible||root.minimizedRecovery||activeTransition==="instant")return
  slideDirection=controller.layoutTransitionDirection
  root.hovered="";root.progress=0;root.overlapProgress=({})
  controller.hover("");controller.joystick(-1,0,0,false)
  layoutReveal.start()
 }
 NumberAnimation {
  id:layoutReveal;target:root;property:"revealProgress";from:0;to:1
  duration:controller.transitionDurationMs
  easing.type:Easing.OutCubic
  onStopped:{root.revealProgress=1;root.hovered="";root.progress=0;controller.hover("")}
 }
 onVisibleChanged:if(!visible)layoutReveal.stop()
 property var overlapProgress: ({})
 readonly property bool overlapEnabled:controller.layout.layoutMode==="canvas"&&controller.layout.activateoverlappingkeys===true&&!controller.sizeMoveMode
 readonly property var overlapTargets: {
  const revision=hitTestRevision
  if(!controller.desktopActive||!overlapEnabled||!root.visible||controller.targeting||controller.keyboardTemporarilyHidden)return []
  if(root.usingGaze&&!root.gazeValid)return []
  if(!root.usingGaze&&!canvasHover.hovered)return []
  const point=root.usingGaze?Qt.point(gaze.x*screenWidth-logicalX,gaze.y*screenHeight-logicalY):canvasHover.point.position
  return keysAt(point.x,point.y)
 }
 property bool gazeEnabled: controller.gazeEnabled
 property bool gazeCheck: false
 readonly property bool restoreButtonEnabled:Qt.platform.os === "linux"
 property bool minimizedRecovery:restoreButtonEnabled&&controller.layoutName.startsWith("minimized-off-screen-")
 property bool trackerConnected:controller.trackerConnected
 property bool desktopActive:controller.desktopActive
 onDesktopActiveChanged:{
  if(!desktopActive){
   root.hovered="";root.progress=0;root.overlapProgress=({});root.targetProgress=0
   root.targetPointValid=false;root.gazeValid=false;root.gaze=Qt.point(-1,-1);root.canvasDragging=false
   keyboardGazeIndicator.resetMotion()
  }
 }
 property bool usingGaze:desktopActive&&gazeEnabled&&trackerConnected
 onUsingGazeChanged:{
  // Require fresh pointer motion after a gaze transition; stale hover must not
  // deflect a joystick again immediately after tracking loss.
  if(root.hovered.startsWith("key:")){
   const item=keys.itemAt(Number(root.hovered.substring(4)))
   if(item&&item.modelData.kind==="Joystick")root.hovered=""
  }
  controller.joystick(-1,0,0,false);controller.hover("");controller.dwellAt(0,0,false);root.progress=0;root.targetProgress=0}
 property bool gazeValid: false
 property point gaze: Qt.point(-1,-1)
 readonly property var themeIds:["system","midnight","ocean","forest","paper","high-contrast"]
 readonly property var themeNames:["System","Midnight","Ocean","Forest","Warm Paper","High Contrast"]
 readonly property var themePalettes:({
  "high-contrast":{background:"#000000",key:"#000000",text:"#ffffff",border:"#ffffff",accent:"#ffff00"},
  midnight:{background:"#111827",key:"#1f2937",text:"#f3f4f6",border:"#64748b",accent:"#c4b5fd"},
  ocean:{background:"#082f49",key:"#0c4a6e",text:"#f0f9ff",border:"#38bdf8",accent:"#67e8f9"},
  forest:{background:"#102b24",key:"#1b4332",text:"#f0fdf4",border:"#6b9e80",accent:"#a7f3d0"},
  paper:{background:"#e9dfcd",key:"#fff8eb",text:"#3d3026",border:"#95816b",accent:"#92400e"}
 })
 readonly property var keyboardPalette:themePalettes[controller.keyboardTheme]||null
 readonly property color keyboardBackground:keyboardPalette?keyboardPalette.background:Kirigami.Theme.backgroundColor
 readonly property color keyboardForeground:keyboardPalette?keyboardPalette.text:Kirigami.Theme.textColor
 readonly property color keyboardBorder:keyboardPalette?keyboardPalette.border:Kirigami.Theme.disabledTextColor
 property color dwellColor:keyboardPalette?keyboardPalette.accent:Kirigami.Theme.highlightColor
 // Geometry is supplied by the platform adapter; Wayland does not expose reliable global window positions.
 property real screenLeft: 0
 property real screenTop: 0
 property real screenWidth: 1100
 property real screenHeight: 900
 property real logicalX: 0
 property real logicalY: 530
 property real targetProgress:0
 property point targetPoint:Qt.point(-1,-1)
 property bool targetPointValid:false
 // Repeater lifecycle changes must invalidate the shared hit even when the
 // replacement layout has the same number of keys.
 readonly property bool transparentBackground: (controller.layout.transparentbackground === "true") || root.minimizedRecovery || (controller.layout.keys||[]).some(k=>root.styleOpacity(k.style,"opacity")<1)
 readonly property bool backgroundClickThrough: transparentBackground && (controller.layout.backgroundclickthrough === "true") && !controller.sizeMoveMode
 onBackgroundClickThroughChanged:Qt.callLater(updateInputMask)
 onWidthChanged:Qt.callLater(updateInputMask)
 onHeightChanged:Qt.callLater(updateInputMask)
 function updateInputMask() {
  let rectangles=[]
  for(let i=0;i<keys.count;++i){let key=keys.itemAt(i);if(key&&key.visible)rectangles.push(Qt.rect(key.x,key.y,key.width,key.height))}
  if(signInTrackerStatus.visible)rectangles.push(Qt.rect(signInTrackerStatus.x,signInTrackerStatus.y,signInTrackerStatus.width,signInTrackerStatus.height))
  controller.updateKeyboardMask(root,rectangles,backgroundClickThrough)
 }
 Component.onCompleted:{displayedLayout=controller.layoutName;Qt.callLater(updateInputMask)}
 property int hitTestRevision: 0
 onHitTestRevisionChanged:Qt.callLater(updateInputMask)
 readonly property string gazeTarget: {
  const revision=hitTestRevision
  return usingGaze&&gazeValid ? keyAt(gaze.x*screenWidth-logicalX,gaze.y*screenHeight-logicalY) : ""
 }
 function styleOpacity(style,name) {
  const value=style&&style[name]!==undefined?Number(style[name]):1
  return isFinite(value)?Math.max(0,Math.min(1,value)):1
 }
 function overSignInStatus(px,py) {
  return signInTrackerStatus.visible&&px>=signInTrackerStatus.x&&py>=signInTrackerStatus.y
     &&px<signInTrackerStatus.x+signInTrackerStatus.width&&py<signInTrackerStatus.y+signInTrackerStatus.height
 }
 function keyAt(px,py) {
  if(root.transitioning)return ""
  if(overSignInStatus(px,py))return ""
  for(let i=keys.count-1;i>=0;--i){let item=keys.itemAt(i);if(!item)continue
   if(px>=item.x&&py>=item.y&&px<item.x+item.width&&py<item.y+item.height){
    if(controller.sizeMoveMode)return "key:"+i
    if(item.modelData.kind==="SuggestionRow") {
     let n=Math.floor((px-item.x)/item.predictionWidth);return n<item.predictionCount&&n<controller.suggestions.length&&controller.suggestions[n].trim().length>0?"suggest:"+n:""
    }
    if(item.modelData.kind==="Scratchpad")return ""
    return "key:"+i
   }
  }
  return ""
 }
 function keysAt(px,py) {
  if(overSignInStatus(px,py))return []
  // Joysticks and prediction slots retain their exclusive interaction model.
  const top=keyAt(px,py)
  if(!top.startsWith("key:"))return []
  const item=keys.itemAt(Number(top.substring(4)))
  if(!item||item.modelData.kind!=="DynamicKey")return []
  let targets=[]
  for(let i=0;i<keys.count;++i){const key=keys.itemAt(i)
   if(key&&key.modelData.kind==="DynamicKey"&&px>=key.x&&py>=key.y&&px<key.x+key.width&&py<key.y+key.height)targets.push("key:"+i)
  }
  return targets
 }
 Connections {
  target:controller
  function onSettingsRequested(){if(controller.signInMode)return;settings.showNormal();settings.raise();settings.requestActivate()}
  function onLayoutChanged(){root.hovered="";root.progress=0;root.revealLayout()}
  function onGazePoint(x,y,valid){keyboardGazeIndicator.updateMotion(x,y,valid);root.gazeValid=valid;root.gaze=Qt.point(x,y)}
  function onChanged(){if(controller.keyboardTemporarilyHidden){root.hovered="";root.progress=0}if(!controller.targeting){root.targetProgress=0;root.targetPointValid=false}}
 }
 Timer {
  interval:25;running:!root.gazeCheck&&root.desktopActive;repeat:true
  onTriggered:{
   if(root.transitioning){controller.hover("");controller.joystick(-1,0,0,false);root.progress=0;root.overlapProgress=({});return}
   if(root.usingGaze&&controller.gazeDwellPaused){controller.pauseGazeDwell();return}
   if(controller.sizeMoveMode){
    const point=root.usingGaze?Qt.point(root.gaze.x*root.screenWidth-root.logicalX,root.gaze.y*root.screenHeight-root.logicalY):canvasHover.point.position
    const valid=root.visible&&!controller.keyboardTemporarilyHidden&&(root.usingGaze?root.gazeValid:canvasHover.hovered)&&!root.canvasDragging
    root.progress=controller.canvasEditAt(point.x/root.width,point.y/root.height,valid)
    root.canvasPoint=point
    return
   }
   let target=controller.keyboardTemporarilyHidden?"":root.hovered
   if(root.usingGaze){
    target=""
    if(root.gazeValid){
     let px=root.gaze.x*root.screenWidth,py=root.gaze.y*root.screenHeight
     let overPanel=root.visible&&px>=root.logicalX&&px<root.logicalX+root.width&&py>=root.logicalY&&py<root.logicalY+root.height
     controller.trackGaze(root.gaze.x,root.gaze.y,!overPanel)
     if(!controller.targeting&&root.visible)target=root.gazeTarget
    }
   }
   if(controller.targeting){
    const point=root.usingGaze?root.gaze:Qt.point(targetPointer.mouseX/targetWindow.width,targetPointer.mouseY/targetWindow.height)
    const valid=root.usingGaze?root.gazeValid:targetPointer.containsMouse&&targetWindow.visible
    target=""
    root.targetPoint=point;root.targetPointValid=valid
    root.targetProgress=controller.dwellAt(point.x,point.y,valid)
   }else{
    root.targetPointValid=false;root.targetProgress=controller.dwellAt(0,0,false)
   }
   let joystickIndex=-1
   if(root.visible&&!controller.targeting&&!controller.keyboardTemporarilyHidden&&target.startsWith("key:")){
    let i=Number(target.substring(4)),item=keys.itemAt(i)
    if(item&&item.modelData.kind==="Joystick"&&!controller.sizeMoveMode){
     joystickIndex=i
     let px=root.usingGaze?root.gaze.x*root.screenWidth-root.logicalX-item.x:item.pointerX
     let py=root.usingGaze?root.gaze.y*root.screenHeight-root.logicalY-item.y:item.pointerY
     item.stickX=Math.max(-1,Math.min(1,(px-item.width/2)/(Math.min(item.width,item.height)*.4)))
     item.stickY=Math.max(-1,Math.min(1,(py-item.height/2)/(Math.min(item.width,item.height)*.4)))
     controller.joystick(i,item.stickX,item.stickY,true)
     target=""
    }
   }
   if(joystickIndex<0)controller.joystick(-1,0,0,false)
   if(root.overlapTargets.length>0){
    root.overlapProgress=controller.hoverKeys(root.overlapTargets)
    root.progress=0
   }else{
    root.overlapProgress=({})
    root.progress=controller.hover(target)
   }
  }
 }
 Rectangle {objectName:"keyboardBackdrop";anchors.fill:parent;visible:!root.transparentBackground;color:root.keyboardBackground}
 Item {
  objectName:"keyboardKeys"
  anchors.fill:parent
  enabled:!root.transitioning
 Repeater {
  id:keys
  onItemAdded: root.hitTestRevision++
  onItemRemoved: root.hitTestRevision++
  model:controller.layout.keys || []
  Rectangle {
   id:key
   onXChanged:Qt.callLater(root.updateInputMask)
   onYChanged:Qt.callLater(root.updateInputMask)
   onWidthChanged:Qt.callLater(root.updateInputMask)
   onHeightChanged:Qt.callLater(root.updateInputMask)
   clip:true
   required property var modelData
   required property int index
   readonly property real reveal:root.activeTransition==="rows"?Math.max(0,Math.min(1,root.revealProgress*2-modelData.row/Math.max(1,controller.layout.rows-1))):root.revealProgress
   transform:[
    Translate {
     x:root.activeTransition==="slide"?(1-root.revealProgress)*root.width*root.slideDirection:0
     y:root.activeTransition==="lift"?(1-root.revealProgress)*8:0
    },
    Scale {
     origin.x:root.width/2-key.x;origin.y:root.height/2-key.y
     xScale:root.activeTransition==="zoom"?0.98+0.02*root.revealProgress:1
     yScale:xScale
    }
   ]
   property bool keyboardToggled:controller.keyToggled(modelData.commands||[])
   property bool gamepadPressed: {
    const commands=modelData.commands||[]
    return commands.length===1&&commands[0].tag==="Action"&&controller.gamepadHeld(commands[0].text.trim())
   }
   Connections {target:controller;function onChanged(){
    const commands=key.modelData.commands||[]
    key.keyboardToggled=controller.keyToggled(commands)
    key.gamepadPressed=commands.length===1&&commands[0].tag==="Action"&&controller.gamepadHeld(commands[0].text.trim())
   }}
   property real pointerX:0
   property real pointerY:0
   property real stickX:0
   property real stickY:0
   property string command:"key:"+index
   property string modifier:controller.modifierName(modelData.commands || [])
   property string modifierState:modifier?(controller.modifierStates[modifier] || ""):""
   property bool cycleButtonModeKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="CycleButtonMode"
   }
   property bool cycleKeyModeKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="CycleKeyMode"
   }
   property bool magnifierKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="MouseMagnifier"
   }
   property bool magneticCursorKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="MouseMagneticCursor"
   }
   property bool lookScrollKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="LookToScrollActive"
   }
   property string heldStyle:modifierState==="latched"?controller.latchStyle:(modifierState==="locked"||keyboardToggled)?controller.lockStyle:""
   property bool stateFilled:((editAction==="MoveModeToggle"&&controller.canvasEditMode==="move")||(editAction==="SizeModeToggle"&&controller.canvasEditMode==="size"))||keyboardToggled||gamepadPressed||modifierState==="locked"||(magnifierKey&&controller.magnifierEnabled)||(sleepKey&&controller.paused)||(lookScrollKey&&controller.lookScrolling)||(magneticCursorKey&&controller.magneticCursor)
   property real predictionWidth:controller.layout.layoutMode==="canvas"?width/Math.max(1,predictionCount):2*root.width/controller.layout.cols
   property int predictionCount:modelData.suggestionCount!==undefined?modelData.suggestionCount:Math.floor(modelData.width/2)
   property bool sleepKey: {
    const commands=modelData.commands || []
    return commands.length===1&&commands[0].tag==="Action"&&commands[0].text.trim()==="Sleep"&&!unavailableReason
   }
   property bool resumeKey:controller.paused&&sleepKey
   enabled:!controller.paused||sleepKey
   property string unavailableReason:modelData.unavailableReason || ""
   property string label:root.minimizedRecovery?["↘","↙","↗","↖"][controller.restoreCorner]:controller.shifted&&modelData.shiftLabel?modelData.shiftLabel:modelData.label
   property real symbolMargin:controller.layout.symbolmargin!==undefined?Math.max(0,Number(controller.layout.symbolmargin)):12
   property color foreground:root.keyboardForeground
   property real foregroundOpacity:root.styleOpacity(modelData.style,"foregroundopacity")
   property string editAction:(modelData.commands||[]).length===1&&modelData.commands[0].tag==="Action"?modelData.commands[0].text.trim():""
   property bool sizeMoveKey:editAction==="MoveModeToggle"||editAction==="SizeModeToggle"
   property real editX:modelData.col
   property real editY:modelData.row
   property real editWidth:modelData.width
   property real editHeight:modelData.height
   x:(editMouse.pressed?editX:modelData.col)*root.width/controller.layout.cols
   y:(editMouse.pressed?editY:modelData.row)*root.height/controller.layout.rows
   width:(editMouse.pressed?editWidth:modelData.width)*root.width/controller.layout.cols
   height:(editMouse.pressed?editHeight:modelData.height)*root.height/controller.layout.rows
   color:root.keyboardPalette?root.keyboardPalette.key:Kirigami.Theme.backgroundColor
   // Composite overlapping key visuals before applying whole-key transparency.
   layer.enabled:opacity<1
   opacity:key.reveal*(!controller.paused||sleepKey?1:0.35)*(root.minimizedRecovery?controller.restoreOpacity/100:root.styleOpacity(modelData.style,"opacity"))
   Kirigami.Theme.colorSet:Kirigami.Theme.Button
   Kirigami.Theme.inherit:false
   border.width:root.minimizedRecovery?0:1;border.color:root.keyboardBorder
   property bool active:enabled&&(root.overlapTargets.length>0?root.overlapTargets.indexOf(command)>=0:(root.usingGaze ? root.gazeTarget===command : root.hovered===command))
   Rectangle {anchors.fill:parent;color:root.dwellColor;opacity:key.active?0.18:0}
   Rectangle {
    anchors.fill:parent;anchors.margins:2
    visible:!key.heldStyle&&((key.sizeMoveKey&&controller.sizeMoveMode)||key.gamepadPressed||(key.magnifierKey&&controller.magnifierEnabled)||(key.sleepKey&&controller.paused)||(key.lookScrollKey&&controller.lookScrolling)||(key.magneticCursorKey&&controller.magneticCursor))
    color:key.stateFilled?root.dwellColor:"transparent"
    opacity:key.stateFilled?0.25:1
    border.width:controller.sizeMoveSelection===key.index?5:2;border.color:root.dwellColor
   }
   HeldIndicator {anchors.fill:parent;anchors.margins:2;style:key.heldStyle;color:root.dwellColor}
   Text {
    anchors.fill:parent;anchors.margins:12;visible:key.modelData.kind==="Scratchpad";clip:true
    opacity:key.foregroundOpacity
    text:controller.scratchpad;color:key.foreground;wrapMode:Text.Wrap
    font.pixelSize:24*controller.scratchpadTextScale/100;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
   }
   Column {
    anchors.centerIn:parent;spacing:4
    opacity:key.foregroundOpacity
    visible:key.modelData.kind==="DynamicKey"&&!key.unavailableReason
    Image {
     anchors.horizontalCenter:parent.horizontalCenter
     // NoIcon is the pack's X symbol, not an instruction to hide the image.
     visible:!root.minimizedRecovery&&key.modelData.symbol!==""
     width:Math.max(1,Math.min(key.width-2*key.symbolMargin,key.height-2*key.symbolMargin-(key.label?keyLabel.implicitHeight+4:0)))
     height:width
     source:visible?"image://symbols/"+key.modelData.symbol+"/"+encodeURIComponent(key.foreground):""
     sourceSize.width:width*root.screen.devicePixelRatio;sourceSize.height:height*root.screen.devicePixelRatio
     opacity:1
     fillMode:Image.PreserveAspectFit
    }
    Text {
     id:keyLabel
     visible:key.label!=="";text:key.label;color:key.foreground
     opacity:1
     width:Math.max(1,key.width-12)
     font.pixelSize:Math.max(16,Math.min(40,key.height*.32))*(text.length<=3?controller.shortLabelScale:controller.longLabelScale)/100
     minimumPixelSize:8;fontSizeMode:Text.HorizontalFit
     horizontalAlignment:Text.AlignHCenter;anchors.horizontalCenter:parent.horizontalCenter
    }
   }
   Rectangle {
    id:modeBadge
    opacity:key.foregroundOpacity
    objectName:"keyModeBadge:"+key.index
    visible:key.modelData.kind==="DynamicKey"&&!key.unavailableReason&&(key.cycleKeyModeKey||key.cycleButtonModeKey)&&!root.minimizedRecovery
    anchors.horizontalCenter:parent.horizontalCenter
    anchors.bottom:parent.bottom;anchors.bottomMargin:7
    width:Math.max(1,Math.min(key.width-12,modeBadgeText.implicitWidth+16))
    height:Math.max(16,Math.min(26,key.height*.22))
    radius:height/2
    color:key.color;border.width:1;border.color:key.foreground
    Text {
     id:modeBadgeText
     anchors.fill:parent;anchors.leftMargin:6;anchors.rightMargin:6
     text:key.cycleButtonModeKey?controller.buttonMode:controller.keyMode;color:key.foreground
     font.pixelSize:Math.max(10,Math.min(16,key.height*.16));font.bold:true
     minimumPixelSize:8;fontSizeMode:Text.Fit
     horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
    }
   }
   Text {
    anchors.fill:parent;anchors.margins:6
    visible:key.unavailableReason.length>0
    opacity:key.foregroundOpacity
    text:"Unavailable\nin Plasma mode";color:key.foreground
    font.pixelSize:16;minimumPixelSize:10;fontSizeMode:Text.Fit
    horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter
    wrapMode:Text.WordWrap
   }
   Rectangle {
    visible:key.modelData.kind==="Joystick"
    anchors.centerIn:parent;width:Math.min(key.width,key.height)*.8;height:width;radius:width/2
    color:"transparent";border.color:key.foreground;opacity:.8*key.foregroundOpacity
    Rectangle {objectName:"joystickThumb:"+key.index;width:parent.width*.18;height:width;radius:width/2;color:root.dwellColor
     x:(parent.width-width)/2+(key.active?key.stickX:0)*parent.width*.35
     y:(parent.height-height)/2+(key.active?key.stickY:0)*parent.height*.35}
    Text {anchors.horizontalCenter:parent.horizontalCenter;anchors.bottom:parent.bottom;text:key.label||"Joystick";color:key.foreground;font.pixelSize:14}
   }
   MouseArea {
    objectName:"keyMouseArea:"+key.index
    anchors.fill:parent;hoverEnabled:true;enabled:true
    onEntered:root.hovered=(key.modelData.kind==="DynamicKey"||key.modelData.kind==="Joystick")?key.command:""
    onPositionChanged:mouse=>{key.pointerX=mouse.x;key.pointerY=mouse.y;if(key.modelData.kind==="Joystick")root.hovered=key.command}
    onExited:if(root.hovered===key.command)root.hovered=""
    onClicked:if(key.modelData.kind==="DynamicKey")controller.clickCommand(key.command)
   }
   Row {
    anchors.fill:parent;visible:key.modelData.kind==="SuggestionRow"
    Repeater {
     model:key.modelData.kind==="SuggestionRow"?key.predictionCount:0
     Rectangle {
      id:predictionSlot
      required property int index
      property string predictionText:index<controller.suggestions.length?controller.suggestions[index]:""
      property bool hasPrediction:predictionText.trim().length>0
      property string command:"suggest:"+index
      property bool active:key.enabled&&hasPrediction&&(root.usingGaze ? root.gazeTarget===command : root.hovered===command)
      onHasPredictionChanged:if(!hasPrediction&&root.hovered==="suggest:"+index)root.hovered=""
      width:key.predictionWidth;height:key.height;color:"transparent";border.color:root.keyboardBorder
      Rectangle {anchors.fill:parent;color:root.dwellColor;opacity:predictionSlot.active?0.18:0}
      Text {anchors.fill:parent;anchors.margins:4;text:predictionSlot.predictionText;opacity:key.foregroundOpacity;color:key.foreground;font.pixelSize:Math.min(24,key.height*.28)*controller.predictionTextScale/100;fontSizeMode:Text.Fit;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
      MouseArea {objectName:"predictionMouseArea:"+index;anchors.fill:parent;hoverEnabled:true;enabled:predictionSlot.hasPrediction;onEntered:root.hovered="suggest:"+index;onExited:if(root.hovered==="suggest:"+index)root.hovered="";onClicked:controller.clickCommand("suggest:"+index)}
      DwellIndicator {objectName:"predictionProgress:"+index;anchors.left:parent.left;anchors.bottom:parent.bottom;width:progress>0?parent.width:0;height:parent.height;progress:predictionSlot.active?root.progress:0;style:controller.dwellIndicatorStyle;color:root.dwellColor}
     }
    }
   }
   DwellIndicator {anchors.fill:parent;progress:key.active?(root.overlapTargets.length>0?(root.overlapProgress[key.command]||0):root.progress):0;style:controller.dwellIndicatorStyle;color:root.dwellColor}
   Rectangle {
    anchors.fill:parent;visible:controller.sizeMoveMode;color:"transparent"
    border.width:controller.sizeMoveSelection===key.index?5:2;border.color:root.dwellColor
    Text {anchors.right:parent.right;anchors.bottom:parent.bottom;visible:controller.canvasEditMode==="size";text:"◢";font.pixelSize:24;color:root.dwellColor}
   }
   MouseArea {
    id:editMouse
    objectName:"sizeMoveMouseArea:"+key.index
    anchors.fill:parent;enabled:controller.sizeMoveMode;hoverEnabled:true
    cursorShape:controller.canvasEditMode==="size"?Qt.SizeFDiagCursor:Qt.SizeAllCursor
    property point startPoint
    property real startX
    property real startY
    property real startWidth
    property real startHeight
    property bool resizing:false
    property bool moved:false
    onEntered:root.hovered=key.command
    onExited:root.hovered=""
    onPressed:mouse=>{
     startPoint=mapToItem(root.contentItem,mouse.x,mouse.y)
     startX=key.modelData.col;startY=key.modelData.row;startWidth=key.modelData.width;startHeight=key.modelData.height
     key.editX=startX;key.editY=startY;key.editWidth=startWidth;key.editHeight=startHeight
     resizing=controller.canvasEditMode==="size";moved=false;root.canvasDragging=true
     root.hovered=""
    }
    onPositionChanged:mouse=>{
     if(!pressed)return
     const point=mapToItem(root.contentItem,mouse.x,mouse.y)
     if(!moved&&Math.abs(point.x-startPoint.x)+Math.abs(point.y-startPoint.y)<4)return
     moved=true
     const dx=(point.x-startPoint.x)*controller.layout.cols/root.width
     const dy=(point.y-startPoint.y)*controller.layout.rows/root.height
     if(resizing){
      key.editWidth=Math.max(Math.min(startWidth,controller.layout.cols*24/root.width),Math.min(controller.layout.cols-startX,startWidth+dx))
      key.editHeight=Math.max(Math.min(startHeight,controller.layout.rows*24/root.height),Math.min(controller.layout.rows-startY,startHeight+dy))
     }else{
      key.editX=Math.max(0,Math.min(controller.layout.cols-startWidth,startX+dx))
      key.editY=Math.max(0,Math.min(controller.layout.rows-startHeight,startY+dy))
     }
    }
    onCanceled:root.canvasDragging=false
    onReleased:{
     root.canvasDragging=false
     if(moved)controller.setCanvasKeyRect(key.index,key.editX,key.editY,key.editWidth,key.editHeight)
     else controller.clickCommand(key.command)
    }
   }
   Accessible.role:Accessible.Button
   Accessible.description:key.keyboardToggled?"Locked until selected again":key.cycleButtonModeKey?"Button mode: "+controller.buttonMode:key.cycleKeyModeKey?"Key mode: "+controller.keyMode:key.sleepKey?(controller.paused?"Pause on":"Pause off"):key.magnifierKey?(controller.magnifierEnabled?"Magnifier on":"Magnifier off"):key.magneticCursorKey?(controller.magneticCursor?"Magnetic mouse on":"Magnetic mouse off"):key.modifier?(key.modifierState==="locked"?"Locked until selected again":key.modifierState==="latched"?"Latched for the next key":"Off. Select once to latch, twice to lock."):""
   Accessible.name:key.sleepKey?(controller.paused?"Resume input":"Pause input"):root.minimizedRecovery?"Restore keyboard":key.unavailableReason?key.unavailableReason:(key.label||key.modelData.symbol)
   Accessible.onPressAction:if(!root.transitioning)controller.clickCommand(key.command)
  }
 }
 }
 Rectangle {
  id:signInTrackerStatus;objectName:"signInTrackerStatus"
  visible:controller.signInMode&&root.desktopActive&&root.visible&&!root.trackerConnected
  anchors.horizontalCenter:parent.horizontalCenter;y:6;z:90
  width:Math.min(720,Math.max(0,root.width-16));height:signInTrackerText.implicitHeight+12
  radius:6;color:root.keyboardBackground;border.width:1;border.color:root.trackerConnected?root.dwellColor:root.keyboardBorder
  onVisibleChanged:Qt.callLater(root.updateInputMask)
  onWidthChanged:Qt.callLater(root.updateInputMask)
  onHeightChanged:Qt.callLater(root.updateInputMask)
  Text {
   id:signInTrackerText;objectName:"signInTrackerText"
   anchors.left:parent.left;anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;anchors.margins:12
   text:!root.gazeEnabled?"Eye tracking is off — mouse dwell available":root.trackerConnected?"Eye tracking connected":"Eye tracker: "+controller.tobiiStatus
   color:root.keyboardForeground;font.pixelSize:13;wrapMode:Text.Wrap;maximumLineCount:2;elide:Text.ElideRight
   Accessible.role:Accessible.StaticText;Accessible.name:text
  }
  // The status is read-only; never activate the keyboard key beneath it.
  MouseArea {anchors.fill:parent;hoverEnabled:true;acceptedButtons:Qt.AllButtons;onEntered:{root.hovered="";root.progress=0}}
 }
 Rectangle {
  z:45;visible:controller.sizeMoveMode&&controller.canvasEditPreview.col!==undefined
  x:(controller.canvasEditPreview.col||0)*root.width/controller.layout.cols
  y:(controller.canvasEditPreview.row||0)*root.height/controller.layout.rows
  width:(controller.canvasEditPreview.width||0)*root.width/controller.layout.cols
  height:(controller.canvasEditPreview.height||0)*root.height/controller.layout.rows
  color:"transparent";border.width:3;border.color:root.dwellColor
 }
 DwellIndicator {
  z:46;visible:controller.sizeMoveMode
  x:root.canvasPoint.x-24;y:root.canvasPoint.y-24;width:48;height:48
  progress:root.progress;style:"ring";color:root.dwellColor
 }
 Item {
  id:keyboardGazeIndicator;objectName:"keyboardGazeIndicator";visible:root.visible&&root.usingGaze&&!controller.targeting
  anchors.fill:parent;z:100;clip:true
  property real lastTime:0
  property point lastPoint:Qt.point(0,0)
  property point velocity:Qt.point(0,0)
  property bool hasVelocity:false
  property real stretch:0
  property real heading:0
  function resetMotion(){lastTime=0;hasVelocity=false;velocity=Qt.point(0,0);stretch=0;heading=0}
  onVisibleChanged:resetMotion()
  onWidthChanged:resetMotion()
  onHeightChanged:resetMotion()
  Connections {target:controller;function onChanged(){if(!controller.gazeIndicatorMorph)keyboardGazeIndicator.resetMotion()}}
  function updateMotion(x,y,valid){
   if(!valid||!visible||!controller.gazeIndicatorMorph){resetMotion();return}
   const now=Date.now(),point=Qt.point(x*root.screenWidth,y*root.screenHeight),dt=(now-lastTime)/1000
   if(lastTime>0&&dt>=0.004&&dt<=0.2){
    const vx=(point.x-lastPoint.x)/dt,vy=(point.y-lastPoint.y)/dt
    const speed=Math.sqrt(vx*vx+vy*vy)
    const acceleration=hasVelocity?Math.sqrt(Math.pow(vx-velocity.x,2)+Math.pow(vy-velocity.y,2))/dt:0
    stretch=speed>20?Math.min(1,acceleration/18000):0
    if(speed>20)heading=Math.atan2(vy,vx)*180/Math.PI
    velocity=Qt.point(vx,vy);hasVelocity=true
   }else{hasVelocity=false;stretch=0}
   lastTime=now;lastPoint=point
  }
  Rectangle {
   id:gazeMarker;objectName:"gazeMarker"
   property real localX:root.gaze.x*root.screenWidth-root.logicalX
   property real localY:root.gaze.y*root.screenHeight-root.logicalY
   visible:root.gazeValid&&localX>=0&&localX<root.width&&localY>=0&&localY<root.height
   x:localX-width/2;y:localY-height/2
   width:controller.gazeIndicatorSize;height:width;radius:width/2
   color:controller.gazeIndicatorColor;opacity:controller.gazeIndicatorOpacity/100
   antialiasing:true
   property real deformation:controller.gazeIndicatorMorph?keyboardGazeIndicator.stretch:0
   Behavior on deformation {NumberAnimation {duration:100}}
   transform:[
    Scale {origin.x:controller.gazeIndicatorSize/2;origin.y:controller.gazeIndicatorSize/2;xScale:1+0.8*gazeMarker.deformation;yScale:1/(1+0.8*gazeMarker.deformation)},
    Rotation {origin.x:controller.gazeIndicatorSize/2;origin.y:controller.gazeIndicatorSize/2;angle:keyboardGazeIndicator.heading}
   ]
  }
 }
 Window {
  id:settings;onActiveChanged:if(active)controller.refreshWindowsStartup();objectName:"settingsWindow";visible:false;width:Math.min(1100,root.screenWidth);height:Math.min(800,root.screenHeight-60);title:"gaze-grid Settings";color:Kirigami.Theme.backgroundColor
  minimumWidth:Math.min(760,root.screenWidth);minimumHeight:Math.min(500,root.screenHeight-60)
  ColorDialog {
   id:gazeColorDialog
   title:"Gaze indicator color"
   onAccepted:controller.gazeIndicatorColor=selectedColor.toString()
  }
  ColorDialog {
   id:targetProgressColorDialog;title:"Progress color"
   onAccepted:controller.targetProgressColor=selectedColor.toString()
  }
  ColorDialog {
   id:scrollZoneColorDialog;title:"Zone color"
   onAccepted:controller.scrollZoneColor=selectedColor.toString()
  }
  property var categoryNames:["Status","Keyboard","Gaze & dwell","Magnifier","Typing & speech","Conversations","Desktop access","Look to scroll","System"]
  property var categoryIcons:["dialog-information","input-keyboard","input-mouse","zoom-in","edit-text-frame-update","user-identity","preferences-desktop","transform-move","preferences-system"]
  property var categoryDescriptions:["Connection health.","Make your keyboard feel right for you.","Fine-tune eye tracking and selection timing.","Choose how you zoom and select targets.","Personalize prediction and spoken text.","","Manage keyboard and pointer access.","Adjust hands-free scrolling.","Manage application performance."]
  component SettingsButton:Desktop.Button {
   id:buttonControl;implicitHeight:38;implicitWidth:Math.max(88,contentItem.implicitWidth+32);leftPadding:16;rightPadding:16
   contentItem:Text {text:buttonControl.text;color:buttonControl.highlighted?"white":Kirigami.Theme.textColor;font.pixelSize:13;font.weight:Font.Medium;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter;opacity:buttonControl.enabled?1:0.4}
   background:Rectangle {radius:6;color:buttonControl.highlighted?(buttonControl.down?"#2864bd":buttonControl.hovered?"#458bed":"#397de0"):Qt.rgba(0.5,0.5,0.5,buttonControl.down?0.25:buttonControl.hovered?0.18:0.09);border.width:1;border.color:buttonControl.activeFocus?"#599cff":Qt.rgba(0.5,0.5,0.5,0.3)}
  }
  component SettingsComboBox:Desktop.ComboBox {
   id:comboControl;implicitHeight:40;leftPadding:12;rightPadding:34
   contentItem:Text {text:comboControl.displayText;color:Kirigami.Theme.textColor;font.pixelSize:14;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight;opacity:comboControl.enabled?1:0.4}
   indicator:Text {x:parent.width-width-14;anchors.verticalCenter:parent.verticalCenter;text:"\u2304";color:Kirigami.Theme.textColor;font.pixelSize:18}
   background:Rectangle {radius:6;color:Qt.rgba(0.5,0.5,0.5,comboControl.hovered?0.13:0.07);border.width:1;border.color:comboControl.activeFocus?"#599cff":Qt.rgba(0.5,0.5,0.5,0.32)}
  }
  component SettingsCheckBox:Desktop.CheckBox {
   id:checkControl;implicitHeight:36;spacing:10
   indicator:Rectangle {x:0;y:(checkControl.height-height)/2;width:20;height:20;radius:4;color:checkControl.checked?"#397de0":"transparent";border.width:1;border.color:checkControl.activeFocus?"#82b5ff":checkControl.checked?"#397de0":"#808080"
    Text {anchors.centerIn:parent;text:"\u2713";color:"white";visible:checkControl.checked;font.pixelSize:14}
   }
   contentItem:Text {text:checkControl.text;leftPadding:30;color:Kirigami.Theme.textColor;font.pixelSize:14;verticalAlignment:Text.AlignVCenter;wrapMode:Text.Wrap;opacity:checkControl.enabled?1:0.4}
  }
  component SettingsSlider:Desktop.Slider {
   id:sliderControl;implicitHeight:28
   background:Rectangle {x:sliderControl.leftPadding;y:sliderControl.topPadding+(sliderControl.availableHeight-height)/2;width:sliderControl.availableWidth;height:4;radius:2;color:Qt.rgba(0.5,0.5,0.5,0.3)
    Rectangle {width:sliderControl.visualPosition*parent.width;height:parent.height;radius:2;color:"#599cff"}
   }
   handle:Rectangle {x:sliderControl.leftPadding+sliderControl.visualPosition*(sliderControl.availableWidth-width);y:sliderControl.topPadding+(sliderControl.availableHeight-height)/2;width:18;height:18;radius:9;color:sliderControl.pressed?"#397de0":"#dceaff";border.width:sliderControl.activeFocus?3:4;border.color:"#599cff";opacity:sliderControl.enabled?1:0.4}
  }
  component SettingsCard:Desktop.GroupBox {
   padding:20;topPadding:50
   label:Text {x:20;y:17;text:parent.title;color:Kirigami.Theme.textColor;font.pixelSize:15;font.weight:Font.DemiBold}
   background:Rectangle {color:Qt.tint(Kirigami.Theme.backgroundColor,Qt.rgba(0.5,0.5,0.5,0.055));radius:10;border.width:1;border.color:Qt.rgba(0.5,0.5,0.5,0.22)}
  }
  component SettingSlider:ColumnLayout {
   property string label
   property string displayValue
   property alias from:control.from
   property alias to:control.to
   property alias stepSize:control.stepSize
   property alias value:control.value
   signal moved()
   Layout.fillWidth:true;spacing:8
   RowLayout {
    Layout.fillWidth:true
    Desktop.Label {text:label;Layout.fillWidth:true;wrapMode:Text.Wrap}
    Desktop.Label {text:displayValue;font.bold:true;horizontalAlignment:Text.AlignRight}
   }
   SettingsSlider {
    id:control;Layout.fillWidth:true;Accessible.name:label
    onMoved:parent.moved()
   }
  }
  component Tool:SettingsButton {
   property string command:""
   focusPolicy:Qt.NoFocus
   onHoveredChanged:root.hovered=hovered?command:""
   onClicked:controller.clickCommand(command)
   Accessible.name:text
  }

  RowLayout {
   anchors.fill:parent;spacing:0
   Rectangle {
    Layout.preferredWidth:230;Layout.fillHeight:true
    color:Kirigami.Theme.backgroundColor
    Kirigami.Theme.colorSet:Kirigami.Theme.View
    Kirigami.Theme.inherit:false
    ColumnLayout {
     anchors.fill:parent;anchors.margins:Kirigami.Units.largeSpacing;spacing:Kirigami.Units.largeSpacing
     ListView {
     id:settingsCategory;objectName:"settingsCategory"
     Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
     model:settings.categoryNames;currentIndex:1
     Accessible.name:"Settings categories"
     onCurrentIndexChanged:if(settingsScroll.contentItem)settingsScroll.contentItem.contentY=0
     delegate:Desktop.ItemDelegate {
      required property int index
      required property string modelData
      width:ListView.view.width;height:46;text:modelData
      contentItem:Text {text:modelData;leftPadding:14;verticalAlignment:Text.AlignVCenter;font.pixelSize:14;font.weight:parent.highlighted?Font.DemiBold:Font.Normal;color:Kirigami.Theme.textColor}
      background:Rectangle {radius:7;color:parent.highlighted?Qt.rgba(0.25,0.52,0.95,0.20):parent.hovered?Qt.rgba(0.5,0.5,0.5,0.10):"transparent"
       Rectangle {width:3;height:18;radius:1.5;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter;color:"#599cff";visible:parent.parent.highlighted}
      }
      icon.name:settings.categoryIcons[index]
      highlighted:ListView.isCurrentItem
      onClicked:settingsCategory.currentIndex=index
     }
    }
    }
   }
   Kirigami.Separator {Layout.fillHeight:true}
   ColumnLayout {
    Layout.fillWidth:true;Layout.fillHeight:true;Layout.margins:28;spacing:20
    RowLayout {
     Layout.fillWidth:true;Layout.bottomMargin:4
     ColumnLayout {
      Layout.fillWidth:true;spacing:6
      Kirigami.Heading {text:settings.categoryNames[settingsCategory.currentIndex];level:1;font.weight:Font.DemiBold}
      Desktop.Label {visible:text.length>0;text:settings.categoryDescriptions[settingsCategory.currentIndex];opacity:0.6;wrapMode:Text.Wrap;Layout.fillWidth:true}
     }
     Rectangle {
      implicitWidth:inputStatus.implicitWidth+28;implicitHeight:30;radius:15;color:controller.paused?Qt.rgba(0.8,0.6,0.2,0.12):Qt.rgba(0.2,0.75,0.5,0.12)
      Desktop.Label {id:inputStatus;anchors.centerIn:parent;text:controller.paused?"Input paused":"Input active";font.pixelSize:12;color:controller.paused?"#d2a552":"#43b98a"}
     }
    }
   Desktop.ScrollView {
    id:settingsScroll;Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true;ScrollBar.horizontal.policy:ScrollBar.AlwaysOff
    ColumnLayout {
     width:settingsScroll.availableWidth-12;spacing:16
     SettingsCard {
      title:settings.categoryNames[0];visible:settingsCategory.currentIndex==0;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Desktop.Label {objectName:"inputModeStatus";text:!root.gazeEnabled?"Gaze off":root.usingGaze?"Gaze dwell active":"Mouse dwell active — eye tracker disconnected";Layout.fillWidth:true;wrapMode:Text.Wrap}
       Desktop.Label {text:"Desktop control: "+controller.portalStatus;Layout.fillWidth:true;wrapMode:Text.Wrap}
       Desktop.Label {text:"Eye tracker: "+controller.tobiiStatus;Layout.fillWidth:true;wrapMode:Text.Wrap}
       Text {visible:text.length>0;text:controller.magneticCursor?"Cursor follows gaze":"";color:Kirigami.Theme.positiveTextColor}
       Text {visible:text.length>0;text:controller.lookScrolling?"Look to scroll active":"";color:Kirigami.Theme.positiveTextColor}
       Text {visible:text.length>0;text:controller.capturePending?"Waiting for desktop capture":controller.magnifierEnabled?"Magnifier on":"";color:Kirigami.Theme.positiveTextColor}
       Text {visible:text.length>0;text:controller.heldKeys.length?"Held: "+controller.heldKeys.join(", "):"";color:Kirigami.Theme.neutralTextColor}
       Text {visible:text.length>0;text:controller.heldPointerButtons.length?"Mouse held: "+controller.heldPointerButtons.join(", "):"";color:Kirigami.Theme.neutralTextColor}
       Tool {id:recover;text:"Home / recover";command:"Recover"}
       SettingsButton {text:"Quit gaze-grid";icon.name:"application-exit";onClicked:Qt.quit()}
      }
     }
     SettingsCard {
      title:settings.categoryNames[1];visible:settingsCategory.currentIndex==1;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       RowLayout {
        Desktop.Label {text:"Keyboard"}
        SettingsComboBox {
         model:includeHidden.checked?controller.layoutNames:controller.menuLayoutNames
         currentIndex:model.indexOf(controller.layoutName)
         displayText:currentIndex<0?controller.layoutName:currentText
         Layout.fillWidth:true;onActivated:controller.openLayout(currentText)
        }
       }
       SettingsCheckBox {id:includeHidden;text:"Include layouts hidden by the XML pack"}
       SettingsCheckBox {
        visible:controller.layout.layoutMode==="canvas"
        text:"Save move/resize changes to original XML"
        checked:controller.saveCanvasChanges
        onToggled:controller.saveCanvasChanges=checked
       }
       SettingsButton {
        visible:controller.layout.layoutMode==="canvas"
        text:"Save current positions and sizes to XML"
        onClicked:controller.saveCanvasGeometry()
       }
       Desktop.Label {
        visible:controller.layout.layoutMode==="canvas";Layout.fillWidth:true;wrapMode:Text.WordWrap
        text:controller.canvasSaveStatus||"Saving overwrites this keyboard's original XML. Automatic saving applies to completed moves and resizes."
       }
      }
     }
     SettingsCard {
      title:"Appearance";      visible:settingsCategory.currentIndex==1;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Desktop.Label {text:"Keyboard theme"}
       SettingsComboBox {
        objectName:"keyboardThemeSelector";Layout.fillWidth:true
        model:root.themeNames
        currentIndex:Math.max(0,root.themeIds.indexOf(controller.keyboardTheme))
        onActivated:controller.keyboardTheme=root.themeIds[currentIndex]
        Accessible.name:"Keyboard theme"
       }
       Desktop.Label {text:"Transition"}
       SettingsComboBox {
        objectName:"keyboardTransitionSelector";Layout.fillWidth:true;model:root.transitionNames
        currentIndex:Math.max(0,root.transitionIds.indexOf(controller.keyboardTransition))
        onActivated:controller.keyboardTransition=root.transitionIds[currentIndex]
        Accessible.name:"Transition"
       }
       SettingSlider {label:"Duration";displayValue:controller.transitionDurationMs+" ms";from:50;to:1000;stepSize:10;value:controller.transitionDurationMs;enabled:controller.keyboardTransition!=="instant";onMoved:controller.transitionDurationMs=Math.round(value)}
       Desktop.Label {text:"Latch style"}
       SettingsComboBox {
        objectName:"latchStyleSelector";Layout.fillWidth:true;model:["Border Highlight","Solid Fill","Bottom Line","Soft Pulse","Expanding Ripple","Light Sweep","Moving Border"]
        currentIndex:Math.max(0,["outline","fill","underline","pulse","ripple","sweep","animated-outline"].indexOf(controller.latchStyle))
        onActivated:controller.latchStyle=["outline","fill","underline","pulse","ripple","sweep","animated-outline"][currentIndex]
        Accessible.name:"Latch style"
       }
       Rectangle {
        Layout.fillWidth:true;Layout.preferredHeight:60;color:root.keyboardBackground;border.color:root.keyboardBorder;radius:8
        Desktop.Label {anchors.centerIn:parent;text:"Latch preview"}
        HeldIndicator {anchors.fill:parent;anchors.margins:3;style:controller.latchStyle;color:root.dwellColor}
       }
       Desktop.Label {text:"Lock style"}
       SettingsComboBox {
        objectName:"lockStyleSelector";Layout.fillWidth:true;model:["Border Highlight","Solid Fill","Bottom Line","Soft Pulse","Expanding Ripple","Light Sweep","Moving Border"]
        currentIndex:Math.max(0,["outline","fill","underline","pulse","ripple","sweep","animated-outline"].indexOf(controller.lockStyle))
        onActivated:controller.lockStyle=["outline","fill","underline","pulse","ripple","sweep","animated-outline"][currentIndex]
        Accessible.name:"Lock style"
       }
       Rectangle {
        Layout.fillWidth:true;Layout.preferredHeight:60;color:root.keyboardBackground;border.color:root.keyboardBorder;radius:8
        Desktop.Label {anchors.centerIn:parent;text:"Lock preview"}
        HeldIndicator {anchors.fill:parent;anchors.margins:3;style:controller.lockStyle;color:root.dwellColor}
       }
       Desktop.Label {text:"Dwell progress style"}
       SettingsComboBox {
        objectName:"dwellStyleSelector";Layout.fillWidth:true;model:root.dwellStyleNames
        currentIndex:Math.max(0,root.dwellStyleIds.indexOf(controller.dwellIndicatorStyle))
        onActivated:controller.dwellIndicatorStyle=root.dwellStyleIds[currentIndex]
        Accessible.name:"Dwell progress style"
       }
       Rectangle {
        Layout.fillWidth:true;Layout.preferredHeight:80;color:root.keyboardBackground;border.color:root.keyboardBorder;radius:8
        Desktop.Label {anchors.centerIn:parent;text:"Dwell preview";color:Kirigami.Theme.textColor}
        DwellIndicator {
         id:dwellPreview;anchors.fill:parent;style:controller.dwellIndicatorStyle;color:root.dwellColor
         NumberAnimation on progress {from:0;to:1;duration:controller.dwellMs;loops:Animation.Infinite;running:settings.visible&&settingsCategory.currentIndex===1}
        }
       }
       SettingSlider {label:"Keyboard height";displayValue:controller.keyboardScale+"%";from:50;to:150;stepSize:5;value:controller.keyboardScale;onMoved:controller.keyboardScale=value}
       SettingSlider {label:"Short key labels";displayValue:controller.shortLabelScale+"%";from:50;to:200;stepSize:5;value:controller.shortLabelScale;onMoved:controller.shortLabelScale=value}
       SettingSlider {label:"Long key labels";displayValue:controller.longLabelScale+"%";from:50;to:200;stepSize:5;value:controller.longLabelScale;onMoved:controller.longLabelScale=value}
       SettingSlider {label:"Click duration";displayValue:controller.clickDurationMs+" ms";from:0;to:250;stepSize:25;value:controller.clickDurationMs;onMoved:controller.clickDurationMs=Math.round(value/25)*25}
       SettingsCheckBox {text:"Temporarily hide keyboard during mouse actions";checked:controller.hideKeyboardDuringMouseActions;onToggled:controller.hideKeyboardDuringMouseActions=checked}
      }
     }
     SettingsCard {
      title:"Startup";visible:Qt.platform.os==="windows"&&settingsCategory.currentIndex==8;Layout.fillWidth:true
      SettingsCheckBox {
       anchors.fill:parent;objectName:"windowsStartupToggle";text:"Start with Windows";checked:controller.startWithWindows
       onToggled:{controller.configureWindowsStartup(checked);checked=Qt.binding(function(){return controller.startWithWindows})}
      }
     }
     SettingsCard {
      title:"Windows sign-in";visible:Qt.platform.os==="windows"&&settingsCategory.currentIndex==8;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Desktop.Label {text:"Use the current keyboard before entering your Windows password. This applies to this PC and disables prediction on the sign-in screen. Requires the trusted installation and administrator approval.";wrapMode:Text.Wrap;Layout.fillWidth:true}
       SettingsCheckBox {
        objectName:"signInEnabledToggle";text:"Use this keyboard at sign-in";checked:controller.signInEnabled
        onToggled:{controller.configureSignIn(checked);checked=Qt.binding(function(){return controller.signInEnabled})}
       }
       SettingsButton {
        objectName:"syncSignInPreferencesButton";text:"Sync current settings"
        onClicked:controller.syncSignInPreferences()
       }
       Desktop.Label {text:"Copies the current keyboard, appearance, dwell, gaze and magnifier settings to login and secure desktop. Requires administrator approval. Does not change sign-in startup.";wrapMode:Text.Wrap;Layout.fillWidth:true;opacity:0.6}
      }
     }
     SettingsCard {
      title:"Restore button";      visible:root.restoreButtonEnabled&&settingsCategory.currentIndex==1;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       RowLayout {
        Desktop.Label {text:"Restore button corner"}
        SettingsComboBox {model:["Top left","Top right","Bottom left","Bottom right"];currentIndex:controller.restoreCorner;onActivated:controller.restoreCorner=currentIndex}
       }
       SettingSlider {label:"Restore button width";displayValue:controller.restoreWidth+" px";from:40;to:128;stepSize:8;value:controller.restoreWidth;onMoved:controller.restoreWidth=value}
       SettingSlider {label:"Restore button height";displayValue:controller.restoreHeight+" px";from:40;to:128;stepSize:8;value:controller.restoreHeight;onMoved:controller.restoreHeight=value}
       SettingSlider {label:"Restore button opacity";displayValue:controller.restoreOpacity+"%";from:10;to:100;stepSize:5;value:controller.restoreOpacity;onMoved:controller.restoreOpacity=value}
      }
     }
     SettingsCard {
      title:settings.categoryNames[2];visible:settingsCategory.currentIndex==2;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingsCheckBox {text:"Enable gaze input";checked:root.gazeEnabled;enabled:!root.gazeCheck;focusPolicy:Qt.NoFocus;onToggled:controller.setGazeEnabled(checked)}
       SettingsCheckBox {text:"Clamp off-screen gaze for keyboard keys";checked:controller.clampGazeForKeyboard;onToggled:controller.clampGazeForKeyboard=checked}
       SettingsCheckBox {text:"Clamp off-screen gaze for point selection";checked:controller.clampGazeForPointSelection;onToggled:controller.clampGazeForPointSelection=checked}
       Desktop.Label {text:"Clamping places off-screen gaze at the nearest screen edge. When off, coordinates stay unchanged. Off-screen gaze remains valid.";wrapMode:Text.Wrap;Layout.fillWidth:true;opacity:0.6}
       SettingSlider {label:"Invalid gaze tolerance";displayValue:controller.invalidGazeToleranceMs+" ms";from:0;to:1000;stepSize:25;value:controller.invalidGazeToleranceMs;onMoved:controller.invalidGazeToleranceMs=value}
       Desktop.Label {text:"Pause dwell during brief tracking loss. Resume when gaze returns, or reset after this time. 0 ms resets immediately.";wrapMode:Text.Wrap;Layout.fillWidth:true;opacity:0.6}
       Tool {id:tobii;text:"Connect eye tracker";command:"Tobii"}
      }
     }
     SettingsCard {
      title:"Gaze smoothing";      visible:settingsCategory.currentIndex==2;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingsComboBox {model:["OptiKey Kalman","Exponential with saccade snap"];currentIndex:controller.gazeFilterType;onActivated:controller.gazeFilterType=currentIndex}
       SettingSlider {visible:controller.gazeFilterType==1;label:"Filter gain";displayValue:controller.gazeFilterGain.toFixed(2);from:0.01;to:1;stepSize:0.01;value:controller.gazeFilterGain;onMoved:controller.gazeFilterGain=value}
       SettingSlider {visible:controller.gazeFilterType==1;label:"Saccade distance";displayValue:controller.gazeSaccadeDistance.toFixed(2)+" (screen coordinates)";from:0.01;to:1;stepSize:0.01;value:controller.gazeSaccadeDistance;onMoved:controller.gazeSaccadeDistance=value}
       SettingSlider {visible:controller.gazeFilterType==0;label:"Gaze smoothing";displayValue:controller.gazeSmoothingLevel+" - "+["Off","Low","Medium","High","Extreme"][controller.gazeSmoothingLevel];from:0;to:4;stepSize:1;value:controller.gazeSmoothingLevel;onMoved:controller.gazeSmoothingLevel=Math.round(value)}
       SettingsCheckBox {visible:controller.gazeFilterType==0;text:"Smooth when changing gaze target";checked:controller.smoothWhenChangingGazeTarget;enabled:controller.gazeSmoothingLevel>0;onToggled:controller.smoothWhenChangingGazeTarget=checked}
      }
     }
     SettingsCard {
      title:"Gaze indicator";      visible:settingsCategory.currentIndex==2;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingSlider {label:"Size";displayValue:controller.gazeIndicatorSize+" px";from:4;to:120;stepSize:1;value:controller.gazeIndicatorSize;onMoved:controller.gazeIndicatorSize=value}
       SettingSlider {label:"Opacity";displayValue:controller.gazeIndicatorOpacity+"%";from:5;to:100;stepSize:5;value:controller.gazeIndicatorOpacity;onMoved:controller.gazeIndicatorOpacity=value}
       RowLayout {
        Desktop.Label {text:"Color"}
        SettingsButton {
         Accessible.name:"Gaze indicator color"
         text:controller.gazeIndicatorColor
         leftPadding:44
         Rectangle {x:12;anchors.verticalCenter:parent.verticalCenter;width:20;height:20;radius:10;color:controller.gazeIndicatorColor;border.width:1;border.color:Kirigami.Theme.textColor}
         onClicked:{gazeColorDialog.selectedColor=controller.gazeIndicatorColor;gazeColorDialog.open()}
        }
       }
       SettingsCheckBox {text:"Morph with gaze acceleration and direction";checked:controller.gazeIndicatorMorph;onToggled:controller.gazeIndicatorMorph=checked}
      }
     }
     SettingsCard {
      title:"Keyboard selection";      visible:settingsCategory.currentIndex==2;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Tool {text:"Button mode: "+controller.buttonMode+" (cycle)";command:"CycleButtonMode"}
       Tool {text:"Key mode: "+controller.keyMode+" (cycle)";command:"CycleKeyMode"}
       Desktop.Label {text:"Tap → Repeat → Hold → Toggle. Hold presses after dwell and releases when mouse or gaze leaves. Toggle keeps a key pressed until selected again. Applies to all physical keyboard keys except modifiers (Shift, Ctrl, Alt and Super/Windows).";wrapMode:Text.Wrap;Layout.fillWidth:true}
       Desktop.Label {text:"Modes included when cycling key mode (keep at least one selected):";wrapMode:Text.Wrap;Layout.fillWidth:true}
       Repeater {
        model:["Tap","Repeat","Hold","Toggle"]
        delegate:SettingsCheckBox {
         required property string modelData
         objectName:"keyCycleMode"+modelData
         text:modelData
         checked:controller.keyCycleModes.indexOf(modelData)>=0
         enabled:!checked||controller.keyCycleModes.length>1
         onToggled:{
          let modes=controller.keyCycleModes.slice()
          if(checked)modes.push(modelData)
          else modes=modes.filter(function(mode){return mode!==modelData})
          controller.keyCycleModes=modes
         }
        }
       }
       SettingsCheckBox {text:"Preserve toggled keys between layouts";checked:controller.preserveTogglesBetweenLayouts;onToggled:controller.preserveTogglesBetweenLayouts=checked}
       SettingsCheckBox {text:"Cycle modifiers while hovering";checked:controller.cycleModifiersOnHover;onToggled:controller.cycleModifiersOnHover=checked}
       SettingSlider {label:"Repeat rate";displayValue:(1000/controller.repeatIntervalMs).toFixed(1)+" / sec ("+controller.repeatIntervalMs+" ms)";from:100;to:2000;stepSize:50;value:controller.repeatIntervalMs;onMoved:controller.repeatIntervalMs=value;Accessible.name:"Repeat interval in milliseconds"}
       SettingSlider {objectName:"allGroupDwellSlider";label:"All groups";displayValue:controller.dwellMs+" ms";from:250;to:2500;stepSize:50;value:controller.dwellMs;onMoved:controller.setAllGroupDwellTimes(value)}
       SettingSlider {label:"Keyboard dwell";displayValue:controller.dwellMs+" ms";from:250;to:2500;stepSize:50;value:controller.dwellMs;onMoved:controller.dwellMs=value}
       Repeater {
        model:["Alphabet","Numbers","Punctuations","Modifiers","Functions","Predictions","Conversations","Navigation","Editing","Mouse","Gamepad","Others"]
        delegate:SettingSlider {
         required property string modelData
         objectName:"groupDwell"+modelData
         label:modelData
         property int milliseconds:controller.groupDwellTimes[modelData]===undefined?controller.dwellMs:controller.groupDwellTimes[modelData]
         displayValue:milliseconds+" ms"
         from:250;to:2500;stepSize:50;value:milliseconds
         onMoved:controller.setGroupDwellTime(modelData,value)
        }
       }

      }
     }
     SettingsCard {
      title:"Zoom behavior";visible:settingsCategory.currentIndex==3;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingSlider {label:"Magnifier zoom";displayValue:controller.magnifierZoom+"×";from:2;to:8;stepSize:1;value:controller.magnifierZoom;onMoved:controller.magnifierZoom=value}
       RowLayout {
        visible:Qt.platform.os==="windows"
        Desktop.Label {text:"Live refresh"}
        SettingsComboBox {model:["15 Hz","30 Hz","45 Hz","60 Hz"];currentIndex:[15,30,45,60].indexOf(controller.magnifierRefreshHz);onActivated:controller.magnifierRefreshHz=[15,30,45,60][currentIndex]}
       }
       SettingsCheckBox {text:"Progressive zoom";checked:controller.progressiveZoom;onToggled:controller.progressiveZoom=checked}
       SettingSlider {label:"Zoom duration";displayValue:(controller.zoomDurationMs/1000).toFixed(1)+" s";from:500;to:5000;stepSize:100;value:controller.zoomDurationMs;enabled:controller.progressiveZoom;onMoved:controller.zoomDurationMs=value}
      }
     }
     SettingsCard {
      title:"Target selection";      visible:settingsCategory.currentIndex==3;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingsCheckBox {text:"Select pointer targets by dwell";checked:controller.targetDwellEnabled;onToggled:controller.targetDwellEnabled=checked}
       SettingSlider {label:"Start zoom / target dwell";displayValue:controller.targetStartDwellMs+" ms";from:250;to:2500;stepSize:50;value:controller.targetStartDwellMs;enabled:controller.targetDwellEnabled;onMoved:controller.targetStartDwellMs=value}
       SettingSlider {label:"Confirm zoomed target dwell";displayValue:controller.targetConfirmDwellMs+" ms";from:250;to:2500;stepSize:50;value:controller.targetConfirmDwellMs;enabled:controller.targetDwellEnabled;onMoved:controller.targetConfirmDwellMs=value}
       SettingSlider {label:"Target dwell movement tolerance";displayValue:controller.targetDwellRadius+" px";from:10;to:100;stepSize:5;value:controller.targetDwellRadius;enabled:controller.targetDwellEnabled;onMoved:controller.targetDwellRadius=value}
       Desktop.Label {text:"Progress style"}
       SettingsComboBox {
        objectName:"targetProgressStyleSelector";Layout.fillWidth:true;model:["Ring Progress","Pie Progress","Ring Shrink"]
        currentIndex:["ring","pie","shrink"].indexOf(controller.targetProgressStyle)
        onActivated:controller.targetProgressStyle=["ring","pie","shrink"][currentIndex]
        Accessible.name:"Progress style"
       }
       SettingSlider {label:"Size";displayValue:controller.targetProgressSize+" px";from:24;to:160;stepSize:1;value:controller.targetProgressSize;onMoved:controller.targetProgressSize=value}
       SettingSlider {label:"Thickness";displayValue:controller.targetProgressThickness+" px";from:1;to:12;stepSize:1;value:controller.targetProgressThickness;onMoved:controller.targetProgressThickness=value}
       SettingSlider {label:"Opacity";displayValue:controller.targetProgressOpacity+"%";from:5;to:100;stepSize:1;value:controller.targetProgressOpacity;onMoved:controller.targetProgressOpacity=value}
       SettingsButton {
        text:"Color";Accessible.name:"Progress color";leftPadding:44
        Rectangle {x:12;anchors.verticalCenter:parent.verticalCenter;width:20;height:20;radius:10;color:controller.targetProgressColor;border.width:1;border.color:Kirigami.Theme.textColor}
        onClicked:{targetProgressColorDialog.selectedColor=controller.targetProgressColor;targetProgressColorDialog.open()}
       }
       Item {
        Layout.fillWidth:true;implicitHeight:176
        PointProgress {
         anchors.centerIn:parent
         width:controller.targetProgressSize;height:width
         style:controller.targetProgressStyle;color:controller.targetProgressColor
         thickness:controller.targetProgressThickness;opacity:controller.targetProgressOpacity/100
         NumberAnimation on progress {from:0;to:1;duration:controller.targetStartDwellMs;loops:Animation.Infinite;running:settings.visible&&settingsCategory.currentIndex===3}
        }
       }
      }
     }
     SettingsCard {
      title:settings.categoryNames[4];visible:settingsCategory.currentIndex==4&&Qt.platform.os!=="windows";Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingsCheckBox {visible:Qt.platform.os!=="windows";text:"Literal keys (bypass platform mappings)";checked:controller.literal;onToggled:controller.literal=checked}
      }
     }
     SettingsCard {
      title:"Word prediction";      visible:settingsCategory.currentIndex==4;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingSlider {objectName:"predictionTextSizeSlider";label:"Prediction text size";displayValue:controller.predictionTextScale+"%";from:50;to:300;stepSize:5;value:controller.predictionTextScale;onMoved:controller.predictionTextScale=value}
       Desktop.Label {text:"Prediction style"}
       SettingsComboBox {
        objectName:"predictionModeSelector";Layout.fillWidth:true
        model:["Context-aware","Common words","Off"]
        currentIndex:["context","frequency","off"].indexOf(controller.predictionMode)
        onActivated:controller.predictionMode=["context","frequency","off"][currentIndex]
        Accessible.name:"Prediction style"
       }
       Desktop.Label {text:"Show the word I'm typing"}
       SettingsComboBox {
        objectName:"typedWordSuggestionModeSelector";Layout.fillWidth:true
        model:["Always, as the first suggestion","Only when there are no predictions","Never"]
        enabled:controller.predictionMode!=="off"
        currentIndex:["always","fallback","never"].indexOf(controller.typedWordSuggestionMode)
        onActivated:controller.typedWordSuggestionMode=["always","fallback","never"][currentIndex]
        Accessible.name:"Show the word I'm typing"
       }
       SettingsCheckBox {objectName:"suggestSentenceStartersToggle";text:"Suggest sentence starters when text is empty";enabled:controller.predictionMode!=="off";checked:controller.suggestSentenceStarters;onToggled:controller.suggestSentenceStarters=checked}
       SettingsCheckBox {objectName:"addSpaceAfterCompletionToggle";text:"Add a space after word completion";enabled:controller.predictionMode!=="off";checked:controller.addSpaceAfterCompletion;onToggled:controller.addSpaceAfterCompletion=checked}
       SettingsCheckBox {text:"Remove completion space before punctuation";enabled:controller.predictionMode!=="off"&&controller.addSpaceAfterCompletion;checked:controller.removeCompletionSpaceBeforePunctuation;onToggled:controller.removeCompletionSpaceBeforePunctuation=checked}
       SettingsCheckBox {objectName:"predictNextWordToggle";text:"Suggest the next word after a space";enabled:controller.predictionMode!=="off";checked:controller.predictNextWord;onToggled:controller.predictNextWord=checked}
       SettingsCheckBox {text:"Use and learn personal words locally";checked:controller.learning;onToggled:controller.learning=checked}
       SettingsButton {text:"Reset learned words";onClicked:controller.resetLearning()}
      }
     }
     SettingsCard {
      title:"Profiles";objectName:"conversationProfilesCard";visible:settingsCategory.currentIndex==5;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:12
       property var selectedProfile:controller.speechProfiles[controller.activeSpeechProfile]
       id:speechProfileEditor
       Desktop.Label {text:"Active profile";font.bold:true}
       SettingsComboBox {
        objectName:"speechProfileSelector";Layout.fillWidth:true
        model:controller.speechProfiles.map(function(profile){return profile.name})
        currentIndex:controller.activeSpeechProfile
        Accessible.name:"Active speech profile"
        onActivated:controller.activeSpeechProfile=currentIndex
       }
       Desktop.TextField {id:speechProfileName;Layout.fillWidth:true;placeholderText:"Profile name";Accessible.name:"Profile name"}
       Flow {
        Layout.fillWidth:true;spacing:8
        SettingsButton {text:"Create profile";enabled:speechProfileName.text.trim().length>0;onClicked:if(controller.createSpeechProfile(speechProfileName.text))speechProfileName.clear()}
        SettingsButton {text:"Rename";enabled:speechProfileName.text.trim().length>0;onClicked:if(controller.renameSpeechProfile(speechProfileName.text))speechProfileName.clear()}
        SettingsButton {text:"Delete profile";enabled:controller.speechProfiles.length>1;onClicked:controller.deleteSpeechProfile()}
       }
       Kirigami.Separator {Layout.fillWidth:true}
       Desktop.Label {text:"Phrases  "+speechProfileEditor.selectedProfile.items.length+" / 12";font.bold:true}
       Repeater {
        model:speechProfileEditor.selectedProfile.items
        delegate:Rectangle {
         required property int index
         required property var modelData
         Layout.fillWidth:true;implicitHeight:phraseFields.implicitHeight+28
         radius:8;color:Qt.rgba(0.5,0.5,0.5,0.05);border.color:Qt.rgba(0.5,0.5,0.5,0.18)
         ColumnLayout {
          id:phraseFields;anchors.fill:parent;anchors.margins:14;spacing:10
         RowLayout {
          Layout.fillWidth:true
          Desktop.Label {text:index+1;font.bold:true;opacity:0.6;Layout.preferredWidth:24}
          Desktop.TextField {id:itemLabel;Layout.fillWidth:true;text:modelData.label;placeholderText:"Key label";Accessible.name:"Key label "+(index+1)}
          SettingsButton {text:"Apply";enabled:itemLabel.text.trim().length>0&&itemPhrase.text.trim().length>0;onClicked:controller.updateSpeechItem(index,itemLabel.text,itemPhrase.text)}
          SettingsButton {text:"Remove";onClicked:controller.removeSpeechItem(index)}
         }
         Desktop.TextArea {id:itemPhrase;Layout.preferredHeight:Math.max(64,implicitHeight);Layout.fillWidth:true;text:modelData.phrase;placeholderText:"Phrase to speak";wrapMode:Text.Wrap;Accessible.name:"Spoken phrase "+(index+1)}
         }
        }
       }
       Kirigami.Separator {Layout.fillWidth:true}
       Desktop.Label {text:"New phrase";font.bold:true}
       Desktop.TextField {id:newSpeechLabel;Layout.fillWidth:true;placeholderText:"New key label";Accessible.name:"New key label"}
       Desktop.TextArea {id:newSpeechPhrase;Layout.fillWidth:true;placeholderText:"New spoken phrase";wrapMode:Text.Wrap;Accessible.name:"New spoken phrase"}
       SettingsButton {
        text:"Add phrase";highlighted:true;enabled:speechProfileEditor.selectedProfile.items.length<12&&newSpeechLabel.text.trim().length>0&&newSpeechPhrase.text.trim().length>0
        onClicked:if(controller.addSpeechItem(newSpeechLabel.text,newSpeechPhrase.text)){newSpeechLabel.clear();newSpeechPhrase.clear()}
       }
       SettingsButton {text:"Open keyboard";onClicked:{controller.openLayout("keyboard-conversation-bottom",true);settings.close()}}
      }
     }
     SettingsCard {
      title:"Speech";      visible:settingsCategory.currentIndex==4;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Flow {Layout.fillWidth:true;spacing:8
        Tool {id:speakButton;text:"Speak";command:"Speak"}
        Tool {id:stopSpeechButton;text:"Stop speech";command:"StopSpeech"}
       }
       SettingSlider {label:"Scratchpad text size";displayValue:controller.scratchpadTextScale+"%";from:50;to:300;stepSize:5;value:controller.scratchpadTextScale;onMoved:controller.scratchpadTextScale=value}
       Desktop.Label {text:"Scratchpad (text typed by gaze-grid only)"}
       Desktop.TextArea {text:controller.scratchpad;placeholderText:"Scratchpad preview";readOnly:true;font.pixelSize:24*controller.scratchpadTextScale/100;Layout.fillWidth:true;Layout.preferredHeight:Math.max(70,font.pixelSize*3);wrapMode:Text.Wrap}
      }
     }
     SettingsCard {
      title:settings.categoryNames[6];visible:settingsCategory.currentIndex==6;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       Tool {id:authorize;text:"Connect desktop control";command:"Connect"}
       SettingsCheckBox {visible:Qt.platform.os!=="windows";text:"Remember desktop-control permission, if supported";checked:controller.rememberDesktop;onToggled:controller.rememberDesktop=checked}
       SettingsButton {visible:Qt.platform.os!=="windows";text:"Disconnect and forget desktop permission";onClicked:controller.forgetDesktopPermission()}
      }
     }
     SettingsCard {
      title:"Magnetic mouse";visible:settingsCategory.currentIndex==2;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       RowLayout {
        Desktop.Label {text:"Movement"}
        SettingsComboBox {
         objectName:"magneticMouseMovementSelector"
         model:Qt.platform.os==="windows"?["Absolute","Relative"]:["Absolute"]
         currentIndex:controller.magneticMouseRelative&&Qt.platform.os==="windows"?1:0
         Accessible.name:"Magnetic mouse movement"
         onActivated:controller.magneticMouseRelative=currentIndex===1
        }
       }
       Desktop.Label {text:controller.magneticMouseRelative?"Moves the cursor toward your gaze point using relative movement and cursor-position feedback.":"Moves the cursor directly to where you look.";wrapMode:Text.Wrap;Layout.fillWidth:true}
       Desktop.Label {text:Qt.platform.os==="windows"?"Applies immediately. Save preferences to keep this setting after restarting.":"Relative movement requires global cursor-position feedback, which is unavailable through the Linux desktop portal.";wrapMode:Text.Wrap;Layout.fillWidth:true;opacity:0.6}
      }
     }
     SettingsCard {
      title:settings.categoryNames[7];visible:settingsCategory.currentIndex==7;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       SettingSlider {label:"Scroll speed";displayValue:controller.scrollSpeed+"%";from:10;to:300;stepSize:10;value:controller.scrollSpeed;onMoved:controller.scrollSpeed=value}
       SettingSlider {label:"Centre dead zone";displayValue:controller.scrollDeadzone+" px";from:10;to:200;stepSize:5;value:controller.scrollDeadzone;onMoved:controller.scrollDeadzone=value}
       RowLayout {Desktop.Label {text:"Direction"} SettingsComboBox {model:["Both axes","Vertical only","Horizontal only"];currentIndex:controller.scrollAxis;onActivated:controller.scrollAxis=currentIndex}}
       SettingsCheckBox {text:"Clamp gaze";checked:controller.clampScrollGaze;enabled:!controller.cancelScrollOffscreen;onToggled:controller.clampScrollGaze=checked}
       SettingsCheckBox {text:"Cancel off-screen";checked:controller.cancelScrollOffscreen;onToggled:controller.cancelScrollOffscreen=checked}
       SettingsCheckBox {objectName:"showScrollZonesToggle";text:"Show zones";checked:controller.showScrollZones;onToggled:controller.showScrollZones=checked}
       SettingSlider {label:"Zone opacity";displayValue:controller.scrollZoneOpacity+"%";from:5;to:100;stepSize:5;value:controller.scrollZoneOpacity;onMoved:controller.scrollZoneOpacity=value}
       RowLayout {
        Desktop.Label {text:"Zone color"}
        SettingsButton {
         objectName:"scrollZoneColorButton";Accessible.name:"Zone color"
         text:controller.scrollZoneColor;leftPadding:44
         Rectangle {x:12;anchors.verticalCenter:parent.verticalCenter;width:20;height:20;radius:10;color:controller.scrollZoneColor;border.width:1;border.color:Kirigami.Theme.textColor}
         onClicked:{scrollZoneColorDialog.selectedColor=controller.scrollZoneColor;scrollZoneColorDialog.open()}
        }
       }
      }
     }
     SettingsCard {
      title:settings.categoryNames[8];visible:settingsCategory.currentIndex==8;Layout.fillWidth:true
      ColumnLayout {
       anchors.fill:parent;spacing:Kirigami.Units.largeSpacing
       RowLayout {
        Desktop.Label {text:"Process priority"}
        SettingsComboBox {
         model:["Normal","Above normal","High"]
         currentIndex:controller.processPriority
         enabled:Qt.platform.os==="windows"
         Accessible.name:"Process priority"
         onActivated:controller.processPriority=currentIndex
        }
       }
       Desktop.Label {text:Qt.platform.os==="windows"?"Applies immediately. Save preferences to keep this priority after restarting.":"Process priority settings are available on Windows.";wrapMode:Text.Wrap;Layout.fillWidth:true;opacity:0.6}
      }
     }
    }
   }
   Desktop.Label {text:controller.profileStatus;visible:text.length>0;wrapMode:Text.Wrap;Layout.fillWidth:true}
   Kirigami.Separator {Layout.fillWidth:true}
   RowLayout {
    Layout.fillWidth:true;spacing:12
    SettingsButton {text:"Reset preferences";onClicked:controller.resetPreferences()}
    Item {Layout.fillWidth:true}
    SettingsButton {text:"Close";onClicked:settings.close()}
    SettingsButton {text:"Save preferences";icon.name:"document-save";highlighted:true;onClicked:controller.savePreferences()}
   }
  }
 }
 }

 Window {
  id:scrollZonesWindow;objectName:"scrollZonesWindow"
  visible:controller.showScrollZones&&controller.lookScrolling&&controller.desktopActive&&controller.gazeEnabled&&!controller.paused&&!controller.targeting&&!controller.capturePending
  width:root.screenWidth;height:root.screenHeight
  color:"transparent"
  flags:Qt.FramelessWindowHint|Qt.WindowDoesNotAcceptFocus|Qt.WindowStaysOnTopHint|Qt.WindowTransparentForInput|Qt.Tool
  Canvas {
   anchors.fill:parent
   opacity:controller.scrollZoneOpacity/100
   property point centre:controller.scrollCentre
   property real deadzone:controller.scrollDeadzone
   property int axis:controller.scrollAxis
   property color zoneColor:controller.scrollZoneColor
   onCentreChanged:requestPaint()
   onDeadzoneChanged:requestPaint()
   onAxisChanged:requestPaint()
   onZoneColorChanged:requestPaint()
   onVisibleChanged:requestPaint()
   onWidthChanged:requestPaint()
   onHeightChanged:requestPaint()
   onPaint:{
    const ctx=getContext("2d")
    ctx.clearRect(0,0,width,height)
    const left=Math.max(0,Math.min(width,centre.x-deadzone)),right=Math.max(0,Math.min(width,centre.x+deadzone))
    const top=Math.max(0,Math.min(height,centre.y-deadzone)),bottom=Math.max(0,Math.min(height,centre.y+deadzone))
    ctx.fillStyle=zoneColor;ctx.strokeStyle=zoneColor;ctx.lineWidth=2;ctx.globalAlpha=.25
    if(axis!==1){ctx.fillRect(0,0,left,height);ctx.fillRect(right,0,width-right,height)}
    if(axis!==2){ctx.fillRect(0,0,width,top);ctx.fillRect(0,bottom,width,height-bottom)}
    ctx.globalAlpha=1;ctx.beginPath()
    if(axis!==1){ctx.moveTo(left,0);ctx.lineTo(left,height);ctx.moveTo(right,0);ctx.lineTo(right,height)}
    if(axis!==2){ctx.moveTo(0,top);ctx.lineTo(width,top);ctx.moveTo(0,bottom);ctx.lineTo(width,bottom)}
    ctx.stroke()
    ctx.beginPath();ctx.arc(centre.x,centre.y,4,0,2*Math.PI);ctx.fill()
    ctx.font="bold 28px sans-serif";ctx.textAlign="center";ctx.textBaseline="middle"
    if(axis!==1){ctx.fillText("←",left/2,centre.y);ctx.fillText("→",(right+width)/2,centre.y)}
    if(axis!==2){ctx.fillText("↑",centre.x,top/2);ctx.fillText("↓",centre.x,(bottom+height)/2)}
   }
  }
 }
 Window {
  id:targetWindow;objectName:"targetWindow"
  // Keep the Windows surface mapped through capture: remapping it can flash or fade.
  // Other platforms still hide the surface so it cannot enter the screenshot.
  visible:controller.targeting&&(Qt.platform.os==="windows"||!controller.capturePending)
  property bool waitForFreshFrame:false
  property bool frameReady:true
  readonly property bool presentationActive:controller.targeting&&!controller.capturePending
  onPresentationActiveChanged:{if(waitForFreshFrame)frameReady=false;if(presentationActive)requestUpdate()}
  // A mapped surface may still hold the last magnified frame. Expose it only
  // after the renderer has produced the current selection contents.
  opacity:controller.capturePending||!frameReady?0:1
  width:root.screenWidth;height:root.screenHeight
  color:"transparent";flags:Qt.FramelessWindowHint|Qt.WindowDoesNotAcceptFocus|Qt.WindowStaysOnTopHint
  Image {anchors.fill:parent;visible:controller.magnifierReady;source:visible?"image://magnifier/"+controller.magnifierRevision:"";cache:false;fillMode:Image.Stretch}
  MouseArea {id:targetPointer;objectName:"targetPointer";anchors.fill:parent;hoverEnabled:true;onClicked:mouse=>controller.targetAt(mouse.x/width,mouse.y/height)}
  PointProgress {
   objectName:"targetProgressRing"
   visible:controller.targeting&&root.targetPointValid&&!controller.capturePending&&!controller.zoomAnimating
   x:root.targetPoint.x*targetWindow.width-width/2;y:root.targetPoint.y*targetWindow.height-height/2
   width:controller.targetProgressSize;height:width
   progress:root.targetProgress;style:controller.targetProgressStyle
   color:controller.targetProgressColor;thickness:controller.targetProgressThickness
   opacity:controller.targetProgressOpacity/100
  }
 }
}
