import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import org.kde.mauikit 1.0 as Maui
import org.kde.kirigami 2.7 as Kirigami

ColumnLayout
{
	id: layout
	anchors.fill: parent
	
	Item
	{
		Layout.fillWidth: true
		Layout.fillHeight: true
		Layout.margins: 0
//		Layout.maximumHeight: parent.height * 0.7
//		Layout.preferredHeight: parent.height * 0.7
//		Layout.minimumHeight: parent.height * 0.7
		
		Maui.ImageViewer
		{
// 			anchors.centerIn: parent
// 			horizontalAlignment: Qt.AlignHCenter
// 			verticalAlignment: Qt.AlignVCenter
			width: parent.width
			height: parent.height
			image.source: currentUrl.startsWith("file://") ? currentUrl : "file://"+currentUrl			
			// 			fillMode: Image.PreserveAspectCrop
// 			asynchronous: true
// 			sourceSize.height: height
// 			sourceSize.width: width
		}
	}
	
	
	Item
	{
		visible: showInfo
        Layout.fillWidth: visible
        Layout.fillHeight: visible
        Layout.minimumHeight: control.height * 0.3
		
		Kirigami.ScrollablePage
		{
			anchors.fill: parent
			Kirigami.Theme.backgroundColor: "transparent"
            padding: 0
            leftPadding: padding
            rightPadding: padding
            topPadding: padding
            bottomPadding: padding
            
			ColumnLayout
			{
				id: _columnInfo
				spacing: space.large
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.mime
						text: qsTr("Type")
						font.pointSize: fontSizes.default
						font.weight: Font.Light
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.mime
						color: Kirigami.Theme.textColor
						
					}
				}
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.date						
						text: qsTr("Date")
						font.pointSize: fontSizes.default
						font.weight: Font.Light	
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.date
						color: Kirigami.Theme.textColor
						
					}
				}
				
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.modified						
						text: qsTr("Modified")
						font.pointSize: fontSizes.default
						font.weight: Font.Light
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.modified
						color: Kirigami.Theme.textColor
						
					}
				}
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.owner						
						text: qsTr("Owner")
						font.pointSize: fontSizes.default
						font.weight: Font.Light
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.owner
						color: Kirigami.Theme.textColor
						
					}
				}
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.tags
						text: qsTr("Tags")
						font.pointSize: fontSizes.default
						font.weight: Font.Light
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.tags
						color: Kirigami.Theme.textColor
						
					}
				}
				
				Column
				{
					Layout.fillWidth: true
					spacing: space.small
					Label
					{
						visible: iteminfo.permissions						
						text: qsTr("Permissions")
						font.pointSize: fontSizes.default
						font.weight: Font.Light
						color: Kirigami.Theme.textColor
						
					}
					
					Label
					{							 
						horizontalAlignment: Qt.AlignHCenter
						verticalAlignment: Qt.AlignVCenter
						elide: Qt.ElideRight
						wrapMode: Text.Wrap
						font.pointSize: fontSizes.big
						font.weight: Font.Bold
						font.bold: true
						text: iteminfo.permissions
						color: Kirigami.Theme.textColor
						
					}
				}
			}
			
		}
	}
	
}

