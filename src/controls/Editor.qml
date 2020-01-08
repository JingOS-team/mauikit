import QtQuick 2.10
import QtQuick.Controls 2.10
import QtQuick.Layouts 1.3
import org.kde.mauikit 1.0 as Maui
import org.kde.kirigami 2.7 as Kirigami
import "private"

Maui.Page
{
	id: control
	Kirigami.Theme.inherit: false
	Kirigami.Theme.colorSet: Kirigami.Theme.View
	
	property bool showLineCount : true
	property bool showSyntaxHighlighting: true
	
	property alias body : body
	property alias document : document
	property alias scrollView: _scrollView
	
	property alias text: body.text
	property alias uppercase: document.uppercase
	property alias underline: document.underline
	property alias italic: document.italic
	property alias bold: document.bold
	property alias canRedo: body.canRedo
		
	Maui.DocumentHandler
	{
		id: document
		document: body.textDocument
		cursorPosition: body.cursorPosition
		selectionStart: body.selectionStart
		selectionEnd: body.selectionEnd
		textColor: control.Kirigami.Theme.textColor
		backgroundColor: control.Kirigami.Theme.backgroundColor
		
		onError:
		{
			body.text = message
			body.visible = true
		}
	}
	
	Row
	{
		z: _scrollView.z +1
		visible: showLineCount
		anchors
		{
			right: parent.right
			bottom: parent.bottom
			margins: Maui.Style.space.big
		}
		
		width: implicitWidth
		height: implicitHeight
		
		Label
		{
			text: body.length + " / " + body.lineCount
			color: control.Kirigami.Theme.textColor
			opacity: 0.5
			font.pointSize: Maui.Style.fontSizes.medium
		}		
	}
	
	Menu
	{
		id: documentMenu
		z: 999
		
		MenuItem
		{
			text: qsTr("Copy")
			onTriggered: body.copy()
			enabled: body.selectedText.length
		}
		
		MenuItem
		{
			text: qsTr("Cut")
			onTriggered: body.cut()	
			enabled: !body.readOnly && body.selectedText.length					
		}
		
		MenuItem
		{
			text: qsTr("Paste")
			onTriggered: body.paste()
			enabled: !body.readOnly					
		}
		
		MenuItem
		{
			text: qsTr("Select all")
			onTriggered: body.selectAll()
		}
		
		MenuItem
		{
			text: qsTr("Web search")
			onTriggered: Qt.openUrlExternally("https://www.google.com/search?q="+body.selectedText)
			enabled: body.selectedText.length			
		}
	}	
	
	
	headBar.visible: !body.readOnly
	
	headBar.leftContent: [				
	
		ToolButton
		{
			icon.name: "edit-undo"
			enabled: body.canUndo
			onClicked: body.undo()
			opacity: enabled ? 1 : 0.5			
		},
		
		ToolButton
		{
			icon.name: "edit-redo"
			enabled: body.canRedo
			onClicked: body.redo()
			opacity: enabled ? 1 : 0.5
		},
		
		Row
		{
			id: _editingActions
			visible: document.isRich && !body.readOnly
			
			ToolButton
			{
				icon.name: "format-text-bold"
				focusPolicy: Qt.TabFocus
				icon.color: checked ? control.Kirigami.Theme.highlightColor : control.Kirigami.Theme.textColor
				checkable: false
				checked: document.bold
				onClicked: document.bold = !document.bold
			}
			
			ToolButton
			{
				icon.name: "format-text-italic"
				icon.color: checked ? control.Kirigami.Theme.highlightColor : control.Kirigami.Theme.textColor
				focusPolicy: Qt.TabFocus
				checkable: false
				checked: document.italic
				onClicked: document.italic = !document.italic
			}
			
			ToolButton
			{
				icon.name: "format-text-underline"
				icon.color: checked ? control.Kirigami.Theme.highlightColor : control.Kirigami.Theme.textColor
				focusPolicy: Qt.TabFocus
				checkable: true
				checked: document.underline
				onClicked: document.underline = !document.underline
			}
			
			ToolButton
			{
				icon.name: "format-text-uppercase"
				icon.color: checked ? control.Kirigami.Theme.highlightColor : control.Kirigami.Theme.textColor
				focusPolicy: Qt.TabFocus
				checkable: true
				checked: document.uppercase
				onClicked: document.uppercase = !document.uppercase
			}					
		}
		]		
	
// 	footBar.visible: !body.readOnly
	footBar.rightContent: [
	ToolButton
	{
		icon.name: "zoom-in"
		onClicked: zoomIn()
	},
	
	ToolButton
	{
		icon.name: "zoom-out"
		onClicked: zoomOut()
	},
	
	ComboBox
	{
		visible: control.showSyntaxHighlighting
		id: languagesListComboBox
		model: document.getLanguageNameList()
		onCurrentIndexChanged: document.formatName = languagesListComboBox.model[currentIndex]		
	}	
	]
	
	ColumnLayout
	{
		anchors.fill: parent
		spacing: 0
		
		Repeater
		{
			model: document.alerts
			
			Maui.ToolBar
			{
				property var alert : model.alert
				
				Layout.fillWidth: true
				
				Kirigami.Theme.backgroundColor: 
				{
					switch(alert.level)
					{
						case 0: return Kirigami.Theme.positiveTextColor
						case 1: return Kirigami.Theme.neutralTextColor
						case 2: return Kirigami.Theme.negativeTextColor							
					}
				}
				
				leftContent: Maui.ListItemTemplate
				{
					Layout.fillWidth: true
					Layout.fillHeight: true
					
					label1.text: alert.title
					label2.text: alert.body
				}
				
				rightContent: Repeater
				{
					model: alert.actionLabels()
					
					Button
					{
						text: modelData
						onClicked: alert.triggerAction(index)
						
						Kirigami.Theme.backgroundColor: Qt.rgba(Kirigami.Theme.backgroundColor.r, Kirigami.Theme.backgroundColor.g, Kirigami.Theme.backgroundColor.b, 0.2)
						Kirigami.Theme.textColor: Kirigami.Theme.textColor
					}
				}
			}
		}
		
		Kirigami.ScrollablePage
		{
			id: _scrollView
			Layout.fillWidth: true
			Layout.fillHeight: true
			
			contentWidth: width
			contentHeight: body.implicitHeight
			
			leftPadding: 0
			rightPadding: 0
			topPadding: 0
			bottomPadding: 0
			
			TextArea
			{
				id: body
				text: document.text				
				font.family: languagesListComboBox.currentIndex > 0 ? "Monospace" : undefined		
				placeholderText: qsTr("Body")
				selectByKeyboard :!Kirigami.Settings.isMobile
				selectByMouse : !Kirigami.Settings.isMobile
				textFormat: TextEdit.AutoText
				palette.text: control.Kirigami.Theme.textColor
				color: control.Kirigami.Theme.textColor
				
				font.pointSize: Maui.Style.fontSizes.large
				wrapMode: TextEdit.WrapAnywhere
				
				activeFocusOnPress: true
				activeFocusOnTab: true
				persistentSelection: true
				
				background: Rectangle
				{
					color: document.backgroundColor
					implicitWidth: body.implicitWidth
					implicitHeight: Math.max(body.implicitHeight, control.height)
				}
				
				// 			onPressAndHold: isMobile ? documentMenu.popup() : undefined				
				
				onPressed:
				{
					if(!Kirigami.Settings.isMobile && event.button === Qt.RightButton)
						documentMenu.popup()
				}
			}
			// 		ScrollBar.vertical.height: _scrollView.height - body.topPadding
			// 		ScrollBar.vertical.y: body.topPadding
		}
	}
	
	
	
	function zoomIn()
	{
		body.font.pointSize = body.font.pointSize + 2
	}
	
	function zoomOut()
	{
		body.font.pointSize = body.font.pointSize - 2
		
	}
}
