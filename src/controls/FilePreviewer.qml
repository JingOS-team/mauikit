
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import org.kde.kirigami 2.0 as Kirigami
import org.kde.mauikit 1.0 as Maui
import "private"

Maui.Dialog
{
	id: control
	
	property string currentUrl: ""
	property var iteminfo : ({})
	property bool isDir : false
	property string mimetype : ""
	property bool showInfo: true
	
	maxHeight: unit * 800
	maxWidth: unit * 500
	defaultButtons: false
		
		footBar.leftContent: Maui.ToolButton
		{
			
			iconName: "document-open"
			onClicked:
			{
				if(typeof(previewLoader.item.player) !== "undefined")
					previewLoader.item.player.stop()
					openFile(currentUrl)
			}
		}
		
		footBar.middleContent: [		
		
		Maui.ToolButton
		{
			visible: !isDir
			
			iconName: "document-share"
			onClicked:
			{
				isAndroid ? Maui.Android.shareDialog(currentUrl) :
				shareDialog.show([currentUrl])
				close()
			}
		},
		
		Maui.ToolButton
		{
			iconName: "love"
		},
		
		Maui.ToolButton
		{
			iconName: "documentinfo"
			checkable: true
			checked: showInfo
			onClicked: showInfo = !showInfo
		}
		]
		
		footBar.rightContent:  Maui.ToolButton
		{
			iconName: "archive-remove"
			onClicked:
			{
				close()
				remove([currentUrl])
			}
		}
		
		Component
		{
			id: imagePreview
			ImagePreview
			{
				id: imagePreviewer
			}
		}
		
		Component
		{
			id: defaultPreview
			DefaultPreview
			{
				id: defaultPreviewer
			}
		}
		
		Component
		{
			id: audioPreview
			AudioPreview
			{
				id: audioPreviewer
			}
		}
		
		Component
		{
			id: videoPreview
			
			VideoPreview
			{
				id: videoPreviewer
			}
		}
		
		ColumnLayout
		{
			anchors.fill: parent			
			spacing: 0
			clip: true
			
			Loader
			{
				id: previewLoader
				Layout.fillWidth: true
				Layout.fillHeight: true
				sourceComponent: switch(mimetype)
				{
					case "audio" :
						audioPreview
						break
					case "video" :
						videoPreview
						break
					case "text" :
						defaultPreview
						break
					case "image" :
						imagePreview
						break
					case "inode" :
					default:
						defaultPreview
				}
			}
			
			Maui.TagsBar
			{
				id: _tagsBar
				Layout.fillWidth: true
				Layout.margins: 0
// 				height: 64
				list.urls: [control.currentUrl]
				allowEditMode: true
				clip: true
				onTagRemovedClicked: list.removeFromUrls(index)
				onTagsEdited: list.updateToUrls(tags)
				
				onAddClicked: 
				{
					dialogLoader.sourceComponent = tagsDialogComponent					
					dialog.composerList.urls = [control.currentUrl]
					dialog.open()
				}
				
			}
		}
		
		onClosed:
		{
			if(previewLoader.item.player)
				previewLoader.item.player.stop()
		}
		
		function show(path)
		{
			control.currentUrl = path
			control.iteminfo = Maui.FM.getFileInfo(path)
			control.mimetype = iteminfo.mime.slice(0, iteminfo.mime.indexOf("/"))
			control.isDir = mimetype === "inode"
			
			showInfo = mimetype === "image" || mimetype === "video" ? false : true
			
			console.log("MIME TYPE FOR PREVEIWER", mimetype, iteminfo.icon)
			open()
		}
}
