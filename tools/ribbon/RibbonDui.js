// This script Pre-processes an enhanced .Ribbon markup file and
// generates .post.Ribbon UICC/IntentCL compatible markup (Ribbon control markup compiler)
// generates DUI compatible Ribbon.rcxml file containing string and bitmap resources
// generates .new.Ribbon which is identical to input .Ribbon file with the addition of any generated Id="" values

function ShowSyntax()
{
    WScript.Echo("syntax: cscript " + WScript.ScriptName + " {path to .Ribbon XML file}\n" +
                 " [/build:Debug] [/rcxml:dirpath] [/include:dirpath] [/export:dirpath]\n" +
                 " [/mappings:dirpath] [/ignoreid] [/obj:dirpath]");
    WScript.Quit(87);
}

/////////////////////////////////////////////
// Constants
/////////////////////////////////////////////

// These nodeType definitions are listed at http://msdn.microsoft.com/en-us/library/system.xml.xmlnodetype.aspx.
var XmlNodeType_Text = 3;
var XmlNodeType_Comment = 8;

/////////////////////////////////////////////
// globals
/////////////////////////////////////////////

// regular expression to extract meaningful names out of CommandName values
// assumes CommandName looks like cmdShare, etc.
var reCommandName2Name = /^[a-z]+([A-Z0-9].+)$/   // skip leading lower-case letters

// constants
var ksIndent = "    ";

// Ranges for RCXML resources
var nRcxmlRangeMin = 10000;
var nRcxmlRangeMax = 19999;

// nodes we treat as root nodes for purposes of assigning Id values in order to cluster Id values together
var arRootInit = new Array();
arRootInit["ApplicationMenu"]        = 10000;
arRootInit["TabGroup"]               = 10400;
arRootInit["Tab"]                    = "TabGroup"; // when we see <Tab>, if it's not under a <TabGroup>, treat it like a <TabGroup> as far as generated Ids are concerned
arRootInit["HelpButton"]             = 17990;
arRootInit["ContextMap"]             = 18000;
arRootInit["QuickAccessToolbar"]     = 19800;

// delta between the initial value specified by the first instance of a root node and the 2nd and later instances
// all entries in arRootInit[] above that have numeric values must have a matching arRootDelta[] entry here
var arRootDelta = new Array();
arRootDelta["ApplicationMenu"] = 1;
arRootDelta["TabGroup"] = 400;      // enough room for the tab plus 99 chunks, *Gallery, MenuCategory, Button, etc. per Tab (nCmdDelta = 4 IDs per entry)
arRootDelta["ContextMap"] = 80;     // enough room for ContextMenu + 19 buttons, etc. (nCmdDelta = 4 IDs per entry)
arRootDelta["HelpButton"] = 1;
arRootDelta["QuickAccessToolbar"] = 1;

// delta between Id values of non-root node
var nCmdDelta = 4;                  // 4 possible STRINGTABLE entries per Command (Label, KeyTip, ToolTip-Title, ToolTip-Description)

// Elements we assume have bitmaps   1=small 2=large
//at some point need to deal with large and small end to end
var arBmpNodes = new Array();
arBmpNodes["Button"] = 2;
arBmpNodes["ComboBox"] = 1;
arBmpNodes["DropDownButton"] = 2;
arBmpNodes["DropDownColorPicker"] = 1;
arBmpNodes["DropDownGallery"] = 2;
arBmpNodes["InRibbonGallery"] = 1;
// not "SplitButton" because its icon comes from a child "Button"
arBmpNodes["Spinner"] = 1;
arBmpNodes["SplitButtonGallery"] = 2;
arBmpNodes["ToggleButton"] = 2;
arBmpNodes["Item"] = 2;             // enhanced markup element (not supported by UICC)

var sNamespaceURI = undefined;
var nodeRibbonRoot = null;
var nodeCommandsRoot = null;
var nodeRCRoot = null;

var arItemListCmds = null;
var arCategoryList = null;
var nCategoryCount = 0;
var arItemListRanges = new Array(); // holds min, max and regexp string of command Id values for each list of <Item> elements we add an RCData entry for

var sRootElementName = "";          // holds the current root element name (used as a key into arRootID[])
var arRootID = new Array();         // holds next Id value to be tried under a given root node name
var arRootMaxID = new Array();      // holds the maximum Id value used for the actual root node name

// Attributes that support Include override
var arAttributeOverrides = new Array("ApplicationModes");

// XML optionally written by GenerateImageMappings()
var strImageMappingNodes = "";
var arImageMappingHash = new Array();

/////////////////////////////////////////////
// Main entry point
/////////////////////////////////////////////

// we must have one unnamed argument (pathname to the .Ribbon source file)
if(WScript.Arguments.Unnamed.length != 1)
{
    ShowSyntax();   // never returns
}

var argsNamed   = WScript.Arguments.Named;      // args starting with /
WScript.Quit(PreprocessRibbonMarkup(WScript.Arguments.Unnamed(0),
    argsNamed.Item("build"), argsNamed.Item("rcxml"), argsNamed.Item("include"), argsNamed.Item("export"),
    argsNamed.Item("mappings"), argsNamed.Exists("ignoreid"), argsNamed.Item("obj")));

/////////////////////////////////////////////
// Functions
/////////////////////////////////////////////

function CreateXmlDocument ()
{
    var xmlNew = new ActiveXObject("msxml2.DOMDocument.6.0");
    xmlNew.async = false;
    xmlNew.validateOnParse = false;
    xmlNew.resolveExternals = false;
    xmlNew.preserveWhiteSpace = true;
    return xmlNew;
}

// Take a command name such as "ID_NEW_MAIL_MESSAGE"
// and return a new string that looks like "NewMailMessage"
function AltParseNameFromCommand(sCommandName)
{
    var strResult = StripIDPrefix(sCommandName);

    if(0 < strResult.length)
    {
        // Capitalize the first letter following an underscore (which is removed)
        // and make the rest lower case.
        var strNewResult = '';
        var ch;
        var cch = strResult.length;

        for(var i = 0; i < cch; i++)
        {
            ch = strResult.charAt(i);
            if('_' == ch)
            {
                if(++i == cch)
                {
                    break;
                }

                ch = strResult.charAt(i);
                if('_' == ch)
                {
                    // We don't want double underscores in the name.
                    strNewResult = '';
                    break;
                }
                strNewResult += ch.toUpperCase();
            }
            else
            {
                strNewResult += ch.toLowerCase();
            }
        }
        strResult = strNewResult;
    }

    if(0 == strResult.length)
    {
        strResult = null;
    }

    return strResult;
}

function StripIDPrefix(sCommandName)
{
    var strResult = '';

    // Remove the prefix (excluding the underscore) from the command name.
    if(sCommandName.substring(0,3) == 'ID_')
    {
        strResult = sCommandName.substring(2,sCommandName.length);
    }
    else if(sCommandName.substring(0,4) == 'IDC_')
    {
        strResult = sCommandName.substring(3,sCommandName.length);
    }
    else if(sCommandName.substring(0,4) == 'IDB_')
    {
        strResult = sCommandName.substring(3,sCommandName.length);
    }

    return strResult;
}

function ThrowExtractionParseError(sCommandName)
{
    // 123 = ERROR_INVALID_NAME - The filename, directory name, or volume label syntax is incorrect.
    throw(new Error(123, "CommandName=\"" + sCommandName + "\" doesn't follow either pattern cmdMyCommandName or ID_MY_COMMAND_NAME.  For cmdMyCommandName, the lower case prefix is stripped, and the remainder is used to generate PNG filenames and RCXML identifiers.  For ID_MY_COMMAND_NAME, the \"ID\" prefix and all underscores are removed, and only the letters following an underscore are capitalized."));
}

function ExtractNameFromCommand(sCommandName, fReturnOriginalOnError)
{
    var sName;
    var aResult = reCommandName2Name.exec(sCommandName);
    if(aResult != null)
    {
        sName = aResult[1];
    }
    else
    {
        sName = AltParseNameFromCommand(sCommandName);
    }
    if(sName == null)
    {
        if(fReturnOriginalOnError)
        {
            // Just return the original name unchanged.
            sName = sCommandName;
        }
        else
        {
            ThrowExtractionParseError(sCommandName);
        }
    }
    return sName;
}

// Take a command name such as "cmdMyCommand" and turn it into "cmdMyCommandLarge"
// Take a command name such as "ID_MY_COMMAND" and turn it into "IDB_MY_COMMAND_LARGE"
function GenerateLargeImageName(sCommandName)
{
    var sName;
    var aResult = reCommandName2Name.exec(sCommandName);
    if(aResult != null)
    {
        sName = "cmd" + aResult[1] + "Large";
    }
    else
    {
        sName = StripIDPrefix(sCommandName);
        if(sName.length > 0)
        {
            // At this point, sName begins with an underscore.
            sName = "IDB" + sName + "_LARGE";
        }
        else
        {
            ThrowExtractionParseError(sCommandName);
        }
    }
    return sName;
}

function NormalizePath(sDir, sSourceDir)
{
    if (undefined != sDir)          // we were launched with /<dir-option>:dirpath
    {
        if(!/(^|\\)$/.test(sDir))   // path is not empty and not ending in a \
        {
            sDir += "\\";
        }
    }
    else                            // no path provided so use source .ribbon path
    {
        sDir = sSourceDir;
    }
    return sDir;
}

function RecursivelyRemoveResources(node)
{
    if (node.nodeType != XmlNodeType_Comment)
    {
        // Remove the resource attributes
        node.removeAttribute("LabelTitle");
        node.removeAttribute("LabelDescription");
        node.removeAttribute("TooltipTitle");
        node.removeAttribute("TooltipDescription");
        node.removeAttribute("Keytip");
        node.removeAttribute("Comment");

        // Is this a node for which we would normally generate a bitmap reference?
        if (arBmpNodes[node.nodeName] != undefined)
        {
            // Null the "Bitmap" attribute
            XmlWriteAttr(node, "Bitmap", "");
        }
        else if (null != node.attributes.getNamedItem("Bitmap"))    // Does it have a bitmap reference anyway?
        {
            // We wouldn't normally generate a bitmap, so just remove the attribute.
            node.removeAttribute("Bitmap");
        }

        // Recursively remove resources
        var nlExportChildren = node.childNodes;
        var nodeChild;

        while(null != (nodeChild = nlExportChildren.nextNode()))
        {
            if (nodeChild.nodeType != XmlNodeType_Text)
            {
                RecursivelyRemoveResources(nodeChild);
            }
        }
    }
}

function FindFreeCmdID(nodeNewViews)
{
    var nCmdID;
    var nodeDup;

    nCmdID = arRootID[sRootElementName];
    if(nCmdID == null || nCmdID == undefined)
    {
        nCmdID = nRcxmlRangeMin;
    }

    while(1) // look for <* CommandName="*" Id="{nCmdID}"> that isn't already in use
    {
        // If the command is out of range, then reset it and look for ANYTHING.
        if(nCmdID > nRcxmlRangeMax)
        {
            nCmdID = nRcxmlRangeMin;
        }

        if(arImageMappingHash[nCmdID.toString(10)] == undefined)    // These aren't in the "new" XML.
        {
            nodeDupe = nodeNewViews.selectSingleNode('//*[@CommandName and @Id="' + nCmdID + '"]');
            if(nodeDupe == null)
            {
                break;  // ID not already in use
            }
        }

        // try the next ID
        nCmdID += nCmdDelta;
    }

    // save the next ID for this sub-tree
    arRootID[sRootElementName] = nCmdID + nCmdDelta;

    return nCmdID;
}

function PreprocessRibbonMarkup(sRibbonPath, sBuildFlavor, sRcxmlDir, sIncludeDir, sExportDir, sMappingsDir, fIgnoreId, sObjDir)
{
    try
    {
        var reSplitPath     = /^(.+\\|)(.+?)(\.[^\\:\.]+|)$/i  // 1=path 2=filename 3=ext
        var arSplit = reSplitPath.exec(sRibbonPath);
        var sSourceDir = arSplit[1];
        var sSourceName       = arSplit[2] + arSplit[3];
        var sPostRibbonName   = arSplit[2] + ".post"  + arSplit[3];
        var sNewRibbonName    = arSplit[2] + ".new"   + arSplit[3];
        var sMergeRibbonName  = arSplit[2] + ".merge" + arSplit[3];
        var sErrRibbonName    = arSplit[2] + ".err"   + arSplit[3];
        var sRCName           = "Ribbon.rcxml";

        sRcxmlDir = NormalizePath(sRcxmlDir, sSourceDir);
        sObjDir = NormalizePath(sObjDir, sSourceDir);
        sIncludeDir = NormalizePath(sIncludeDir, sSourceDir);
        sExportDir = NormalizePath(sExportDir, sSourceDir);
        sMappingsDir = NormalizePath(sMappingsDir, "");         // No default path

        var fso = new ActiveXObject("Scripting.FileSystemObject");
        try {fso.DeleteFile(sObjDir + sPostRibbonName);} catch(e){}
        try {fso.DeleteFile(sSourceDir + sNewRibbonName);} catch(e){}
        try {fso.DeleteFile(sObjDir + sMergeRibbonName);} catch(e){}
        try {fso.DeleteFile(sSourceDir + sErrRibbonName);} catch(e){}
        try {fso.DeleteFile(sRcxmlDir + sRCName);} catch(e){}

        var xmlRibbon = CreateXmlDocument();
        if(!xmlRibbon.load(sRibbonPath))
        {
            throw(new Error(xmlRibbon.parseError.errorCode, "loading XML file '" + sRibbonPath + "' - " + xmlRibbon.parseError.reason));
        }

        // .new.ribbon copy of the original .ribbon file that will contain the generated Id="" attributes
        // devs can copy the .new.ribbon over the original file and check in the updated original to preserve assigned Id values
        var xmlNew = CreateXmlDocument();
        if(!xmlNew.load(xmlRibbon))
        {
            throw(new Error(xmlNew.parseError.errorCode, "loading XML file from xmlRibbon - " + xmlNew.parseError.reason));
        }

        var xmlRC = CreateXmlDocument();
        xmlRC.loadXML(
                '<?xml version="1.0" encoding="utf-8"?>\n' +
                '<!--\n' +
                '  Ribbon DUI RCXML (generated by ' + WScript.ScriptName + ' from ' + sSourceName + ')\n' +
                '\n' +
                '  IDs match command IDs in generated ' + sPostRibbonName + '.\n' +
                '-->\n' +
                '<Resources Name="Ribbon" IDRange="' + nRcxmlRangeMin + '-' + nRcxmlRangeMax + '">\n' +
                '\n' +
                '<AddRC Value="// Locale"/>\n' +
                '<AddRC Value="LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US"/>\n' +
                '<AddRC Value="#pragma code_page(1252)"/>\n' +
                '<AddRC Value=" "/>\n' +
                '\n' +
                '<AddH Value=" "/>\n' +
                '<AddH Value="// Ribbon Resource ID Helpers for Stringtable entries"/>\n' +
                '<AddH Value="#define RibbonCommandIdToLabel(CommandId)              ((CommandId) + 0)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToKeytip(CommandId)             ((CommandId) + 1)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToLabelDescription(CommandId)   ((CommandId) + 1)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToTooltipTitle(CommandId)       ((CommandId) + 2)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToTooltipDescription(CommandId) ((CommandId) + 3)"/>\n' +
                '<AddH Value=" "/>\n' +
                '<AddH Value="// Ribbon Resource ID Helpers for PNG entries"/>\n' +
                '<AddH Value="#define RibbonCommandIdToBitmap96(CommandId)           ((CommandId) + 0)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToBitmap144(CommandId)          ((CommandId) + 1)"/>\n' +
                '<AddH Value=" "/>\n' +
                '<AddH Value="// Ribbon Resource ID Helpers for Item and Category Lists"/>\n' +
                '<AddH Value="#define RibbonCommandIdToItemList(CommandId)           ((CommandId) + 0)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToCategoryList(CommandId)       ((CommandId) + 1)"/>\n' +
                '<AddH Value="#define RibbonCommandIdToBitmapInfo(CommandId)         ((CommandId) + 2)"/>\n' +
                '<AddH Value=" "/>\n' +
                '\n' +
                '</Resources>' );

        nodeRCRoot = xmlRC.selectSingleNode("Resources");


        var arCmdStack = new Array();       // used to build comment strings based on node hierarchy

        arItemListCmds = new Array("",0);   // array holds the cmd IDs of the <Item> elements in the current set
        arCategoryList = new Array();

        // retrieves the value of the xmlns attribute of the root element eg <Application xmlns="http://schemas.microsoft.com/windows/2009/Ribbon">
        sNamespaceURI = xmlRibbon.documentElement.namespaceURI;

        // sets n: namespace prefix for the .ribbon file's required namespace so we can refer to n:nodename in our queries
        XmlSetSelectionNamespace(xmlRibbon);
        XmlSetSelectionNamespace(xmlNew);

        var nodeNewViews = xmlNew.selectSingleNode("n:Application/n:Application.Views");

        nodeRibbonRoot = XmlGetElem(xmlRibbon, "Application", "", null, 4);
        nodeCommandsRoot = XmlGetElem(nodeRibbonRoot, "Application.Commands", "", null, 4);

        /**************************************************************************
          STEP 0 - support for <RibbonExport>
         **************************************************************************/

        var nlExport = nodeRibbonRoot.selectNodes("//n:RibbonExport"); // find all Export elements
        var nodeExport;
        while(null != (nodeExport = nlExport.nextNode()))
        {
            var sDest = XmlReadAttr(nodeExport, "Dest", "");    // eg Dest="..\common\SignIn.ribbon"
            if(sDest == "")
            {
                // 8316 = ERROR_DS_MISSING_REQUIRED_ATT - A required attribute is missing.
                throw(new Error(8316, "missing Dest=\"export_path.ribbon\" attribute in " + node.xml));
            }

            WScript.Echo("\nExporting: " + sExportDir + sDest);

            var xmlExport = CreateXmlDocument();

            xmlExport.documentElement = xmlExport.createNode(1/* element node type */, "RibbonInclude", sNamespaceURI);
            XmlSetSelectionNamespace(xmlExport);

            var exportResources = (nodeExport.getAttribute("ExportResources") != "false");
            // Specifying 'ExportResources="false"' in the markup will cause strings and bitmaps to be removed or
            // nulled when writing the exported file.  An application importing these chunks probably wants to reuse
            // these resources through a shared code linkage, rather than adding the resources to its own set.

            // copy child nodes of root <RibbonExport> element to the export file
            var nlExportChildren = nodeExport.childNodes;
            while(null != (node = nlExportChildren.nextNode()))
            {
                if(node.nodeType != XmlNodeType_Text)
                {
                    XmlInsertNode(nodeExport.parentNode, node, 4, null, nodeExport);

                    if(node.nodeName == "RibbonExportIgnore")
                        continue;

                    var importedNode = xmlExport.importNode(node, true);
                    if (!exportResources)
                    {
                        RecursivelyRemoveResources(importedNode);
                    }

                    // remove all <RibbonExportIgnore> elements before we insert the node into the export document
                    XmlRemoveNodes(importedNode.selectNodes(".//n:RibbonExportIgnore"), false);

                    XmlInsertNode(xmlExport.documentElement, importedNode, 4, null, null);
                }
            }

            // remove our extended <RibbonExport> element since its not supported by UICC.exe
            XmlRemoveNode(nodeExport);

            // Save the RibbonExport file
            xmlExport.save(sExportDir + sDest);
        }//while over <RibbonExport> elements

        // Remove any <RibbonExportIgnore> tags while still keeping their children.
        XmlRemoveNodes(nodeRibbonRoot.selectNodes("//n:RibbonExportIgnore"), true);

        /**************************************************************************
          STEP 1 - support for <Include Src="pathname.ribbon" FirstId="12348" ApplicationMode="4" Namespace="Example"/>
         **************************************************************************

           Include file should look like this: 
            <?xml version="1.0" encoding="UTF-8"?>
            <RibbonInclude xmlns="http://schemas.microsoft.com/windows/2009/Ribbon">
                <!-- first line of markup copied into including file -->
                <Group CommandName="cmdIncTestGroup" SizeDefinition="ThreeButtons-OneBigAndTwoSmall" LabelTitle="Include Test">
                    <SplitButton CommandName="cmdIncPasteSplit" Keytip="V" TooltipDescription="ttd">
                        <SplitButton.ButtonItem>
                            <Button CommandName="cmdIncPaste" LabelTitle="Paste" TooltipTitle="ttt" TooltipDescription="ttd"/>
                        </SplitButton.ButtonItem>
                        <SplitButton.MenuGroups>
                            <MenuGroup Class="MajorItems">
                                <Button CommandName="cmdIncPaste"/>
                                <Button CommandName="cmdIncPasteText" LabelTitle="Paste Text" LabelDescription="ld" Bitmap=""/>
                                <Button CommandName="cmdIncPasteNarration" LabelTitle="Paste Narration" LabelDescription="ld" Bitmap=""/>
                            </MenuGroup>
                        </SplitButton.MenuGroups>
                    </SplitButton>
                    <Button CommandName="cmdIncCut" Keytip="X" LabelTitle="Cut" TooltipTitle="No Id" TooltipDescription="Cut the selection from the project and put it on the Clipboard."/>
                    <Button CommandName="cmdIncCopy" Keytip="C" LabelTitle="Copy" TooltipTitle="Copy (Ctrl+C)" TooltipDescription="Copy the selection and put it on the clipboard."/>
                </Group>
                <!-- last line of markup copied into including file -->
            </RibbonInclude>
        */
        var nl = nodeRibbonRoot.selectNodes("//n:Include"); // find all Include elements
        var nodeInclude;
        while(null != (nodeInclude = nl.nextNode()))
        {
            var sSrc = XmlReadAttr(nodeInclude, "Src", "");    // eg Src="..\common\SignIn.ribbon"
            if(sSrc == "")
            {
                // 8316 = ERROR_DS_MISSING_REQUIRED_ATT - A required attribute is missing.
                throw(new Error(8316, "missing Src=\"path.ribbon\" attribute in " + node.xml));
            }

            var xmlInclude = CreateXmlDocument();
            if(!xmlInclude.load(sIncludeDir + sSrc))
            {
                throw(new Error(xmlInclude.parseError.errorCode, "unable to load Include file '" + sIncludeDir + sSrc + "' - " + xmlRibbon.parseError.reason));
            }
            XmlSetSelectionNamespace(xmlInclude);

            var sNamespace = XmlReadAttr(nodeInclude, "Namespace", ""); // eg Namespace="Example"
            if(sNamespace != "")
            {
                SetNamespace(xmlInclude, sNamespace);
            }

            var attributeOverrides = GetAttributeOverrides(nodeInclude);

            var nodeIncRoot = xmlInclude.selectSingleNode("n:RibbonInclude");
            if(nodeIncRoot == null)
            {
                // 14057 = ERROR_SXS_XML_E_MISSINGROOT - Manifest Parse Error : XML document must have a top level element.
                throw(new Error(14057, "missing <RibbonInclude> root element in '" + sIncludeDir + sSrc + "' - make sure xmlns attr matches including .ribbon")); 
            }

            ApplyAttributeOverrides(nodeIncRoot, attributeOverrides);

            arSplit = reSplitPath.exec(fso.GetAbsolutePathName(sIncludeDir + sSrc));
            var sBitmapDir = arSplit[1];
            var sIncludeName = arSplit[2];
            var node;
            var nlInc;

            var arCommandIds = new Array();
            var nFirstId = parseInt(XmlReadAttr(nodeInclude, "FirstId", "0"));
            var nNextId = nFirstId;

            nlInc = nodeIncRoot.selectNodes("//*[@CommandName]"); // find all descendents with a CommandName attribute
            while(null != (node = nlInc.nextNode()))
            {
                // sometimes you specify the same CommandName on multiple items and we only want to assign an Id to the first instance
                var nodeFirst = nodeIncRoot.selectSingleNode("//*[@CommandName = \"" + XmlReadAttr(node, "CommandName", "") + "\"]");
                if(node == nodeFirst)
                {
                    var sCommandName = XmlReadAttr(node, "CommandName", "");

                    if(nFirstId > 0) // if <Include FirstId=""/> specified, set Ids on all elements with CommandName attributes
                    {
                        XmlWriteAttr(node, "Id", nNextId);   // overwrites any existing Id value from the include file - if you don't want this, don't specify FirstId
                        arCommandIds.push(nNextId);
                        nNextId += nCmdDelta;
                    }
                    else
                    {
                        var nCurId = parseInt(XmlReadAttr(node, "Id", "0"));
                        if(nCurId > 0)
                        {
                            arCommandIds.push(nCurId);
                        }
                    }

                    var sBitmap = XmlReadAttr(node, "Bitmap");  // specify Bitmap="" to force no bitmap or Bitmap="file.png" to force a specific file
                    if(sBitmap != undefined)                        // Bitmap attr is present
                    {
                        if(sBitmap != "")       // preserve empty Bitmap="" which signals no bitmap
                        {
                            XmlWriteAttr(node, "Bitmap", sBitmapDir + sBitmap); // convert include file relative to absolute path
                        }
                    }
                    else                                            // Bitmap attr is NOT present
                    {
                        if(arBmpNodes[node.nodeName] != undefined)  // AND this is a node we generate a bitmap reference for
                        {
                            var sName = ExtractNameFromCommand(sCommandName, false);
                            XmlWriteAttr(node, "Bitmap", sBitmapDir + "Ribbon" + sName + ".png");    // use auto generated pathname
                        }
                    }
                }
                else // not first use of CommandName
                {
                    node.removeAttribute("Id");         // ensure non-first items don't have an Id already from the include file
                    node.removeAttribute("Bitmap");     // ensure non-first items don't have a Bitmap already from the include file
                }
            }//while nlInc.nextNode()

            // #defines to help app known which handler to connect to this include's command Ids
            AddRCNode("AddH", null, null, "// Include " + sSrc);
            AddRCNode("AddH", null, null, "#define RibbonInclude_"+sIncludeName+"_FirstId  " + nFirstId);
            AddRCNode("AddH", null, null, "#define RibbonInclude_"+sIncludeName+"_LastId   " + (nNextId - nCmdDelta));
            AddRCNode("AddH", null, null, "#define RibbonInclude_"+sIncludeName+"_IdArray  {" + arCommandIds.join(",") + "}");
            AddRCNode("AddH", null, null, " ");

            XmlInsertCommentBefore(nodeInclude, "## Start " + nodeInclude.xml + " ##", "", "");

            // copy child nodes of root <RibbonInclude> element in the include file immediately before the <Include> element in the master .ribbon file
            nlInc = nodeIncRoot.childNodes;
            while(null != (node = nlInc.nextNode()))
            {
                if(node.nodeType != XmlNodeType_Text)
                {
                    XmlInsertNode(nodeInclude.parentNode, node, 4, null, nodeInclude);
                }
            }

            XmlInsertCommentBefore(nodeInclude, "## End " + nodeInclude.xml + " ##", "", "");

            // remove our extended <Include> element since its not supported by UICC.exe
            XmlRemoveNode(nodeInclude);
        }//while over <Include> elements

        /**************************************************************************
          STEP 2 - support for Build attribute on elements
         **************************************************************************/

        nl = nodeRibbonRoot.selectNodes("n:Application.Views//*[@Build]"); // find all descendents with a Build attribute
        for (var i=0; i < nl.length; i++)
        {
            var node = nl.item(i);

            if (undefined != sBuildFlavor) // we were launched with /build:flavor
            {
                var sBuild = XmlReadAttr(node, "Build", "");    // eg "Debug" or "Retail|Debug" or "Ship|Retail"
                var reBuild = new RegExp("^(" + sBuild + ")$", "i");
                if(!reBuild.test(sBuildFlavor)) // build flavor doesn't match the Build attribute pattern
                {
                    XmlRemoveNode(node);
                    continue;
                }
            }

            // since we didn't remove the node entirely, remove attribute since this is one of our enhanced attributes that UICC/IntentCL doesn't support
            node.attributes.removeNamedItem("Build");
        }

        // save a copy of xmlRibbon now that any <Include> files are merged in and any Build="" elements have been removed
        // this is used by the test automation
        WScript.Echo("\nWriting: " + sObjDir + sMergeRibbonName);
        xmlRibbon.save(sObjDir + sMergeRibbonName);

        // Scan for image mapping attributes, write C++ mapping code, and remove the attributes from the XML.
        var sImageMappings = GenerateImageMappings();

        // If image mapping nodes have been generated, they must be added to the in-memory ribbon XML.
        if(0 < strImageMappingNodes.length)
        {
            var nlRibbon = nodeRibbonRoot.selectNodes("//n:Ribbon");    // There has to be one.
            var nodeFirstRibbon = nlRibbon(0);
            var nodeRoot;
            var xmlImageMappingNodes = CreateXmlDocument();

            xmlImageMappingNodes.loadXML(strImageMappingNodes);
            nodeRoot = xmlImageMappingNodes.selectSingleNode("ExtraCommands");

            // When we look up new command IDs, we have to search under "ExtraCommand" 
            sRootElementName = "ExtraCommand";

            // Copy the extra command nodes into the first ribbon node.
            nlInc = nodeRoot.childNodes;
            while(null != (node = nlInc.nextNode()))
            {
                if(node.nodeType != XmlNodeType_Text)
                {
                    var id = FindFreeCmdID(nodeNewViews);
                    XmlWriteAttr(node, "Id", id);
                    arImageMappingHash[id.toString(10)] = true;

                    // Insert the node into the first ribbon.
                    XmlInsertNode(nodeFirstRibbon, node, 4, null, null);
                }
            }

            strImageMappingNodes = "";
        }

        if(sMappingsDir != "")    // If you don't specify "/mappings" then "RibbonMappings.h" will not be created.
        {
            var sMappingsFile = sMappingsDir + "RibbonMappings.h";
            var oFile;
            var fso = new ActiveXObject("Scripting.FileSystemObject");

            // Delete any mappings file from a previous build.
            try {fso.DeleteFile(sMappingsFile);} catch(e){}

            // Write the new mappings to disk.  Now that all the mapping nodes have also been
            // added to the in-memory ribbon XML, RTL mappings can also be generated.
            oFile = fso.CreateTextFile(sMappingsFile, 2 /* writing */, true);
            oFile.Write(sImageMappings + "\r\n" + BuildRTLMapping());
            oFile.Close();
        }

        /**************************************************************************
          STEP 3 - Expand all elements with CommandName attribute
         **************************************************************************/
         
        nl = nodeRibbonRoot.selectNodes("n:Application.Views//*[@CommandName]"); // find all descendents with a CommandName attribute
        for (var i=0; i < nl.length; i++)
        {
            var node = nl.item(i);

            var sCommandName = XmlReadAttr(node, "CommandName", "");
            if(sCommandName == "")
            {
                // treat CommandName="" as if it wasn't specified - we have no work to do
                continue;
            }

            var sName = ExtractNameFromCommand(sCommandName, false);
            var sCurNodeName = node.nodeName;

            //get depth below <Application.Views>
            var nodeWalk = node;
            var nDepth = -1;
            while(nodeWalk != null && nodeWalk.nodeName != "Application.Views")
            {
                nodeWalk = nodeWalk.parentNode;
                nDepth++;
            }

            var nCmdID = 0;
            var sID;

            // <Application.Views> nodes can specify an Id attr to indicate the <Command Id="val"> they want instead of us generating one
            // this enables preserving Id values, if .new.ribbon is manually copied over .ribbon, even when markup order changes
            if(!fIgnoreId)
            {
                sID = XmlReadAttr(node, "Id");
                if(sID != undefined)
                {
                    node.attributes.removeNamedItem("Id");
                    nCmdID = parseInt(sID);
                }
            }

            // see if we already have a <Command Name="{sCommandName}"> and if so use its Id value.
            // since our .ribbon doesn't include these nodes (normally) this means the sCommandName has been reused so verify different Id values are not specified.
            var nodeCommand = XmlGetElem(nodeCommandsRoot, "Command", "Name", sCommandName, 0);
            if(nodeCommand != null)
            {
                sID = XmlReadAttr(nodeCommand, "Id");
                if(sID != undefined)
                {
                    var nExistingID = parseInt(sID);
                    if( (nCmdID != 0)
                     && (nExistingID != nCmdID) )
                    {
                        // 5924 = ERROR_DEPENDENT_RESOURCE_PROPERTY_CONFLICT - One or more property values for this resource are in conflict with one or more property values associated with its dependent resource(s).
                        throw(new Error(5924, "CommandName=\"" + sCommandName + "\" has conflicting Id values in the <Application.Commands> and <Application.Views> sections.  Recommend manually removing the Id attr from <Application.Commands><Command>."));
                    }
                    nCmdID = nExistingID;
                }
            }

            // determine the initial ID for this node if it is one of our special root nodes as defined by arRootInit[]
            var InitID = undefined;
            while(1)
            {
                InitID = arRootInit[sCurNodeName];
                if(typeof(InitID) == "string")
                {
                    sCurNodeName = InitID;  // handle alias to another node name (ie Tab -> TabGroup)
                    if(node.parentNode.nodeName == InitID)
                    {
                        // if a <Tab> is under a <TabGroup> then the tab isn't a root since we've already processed the <TabGroup> root
                        // so set InitID to undefined to signal the current node isn't a root node
                        InitID = undefined;
                        break;
                    }
                    // if a <Tab> is not under a <TabGroup> then we do have to treat the <Tab> like a <TabGroup> root
                    continue;
                }
                break;  // either sCurNodeName is a root node and we have the InitID OR it wasn't and InitID == undefined
            }

            //TODO write function to find next free Id for a given node type
            // use it for node root and normal nodes
            // inherit next id from parent node or perhaps just previous node

            // if this element is in our array of root element names, we need to start numbering for a new sub-tree
            // otherwise InitID == undefined and we need to use the current sub-tree numbering
            if(undefined != InitID)
            {
                sRootElementName = sCurNodeName;    // this node is a root node so remember for when we process its children
                var nCurMaxRootID = arRootMaxID[sRootElementName];

                var nRootID = nCmdID;
                if(nRootID == 0)
                {
                    //nRootID = arRootID[sCurNodeName];
                    nRootID = nCurMaxRootID;
                    if(nCurMaxRootID == undefined) // first time we've seen this root node type so use the arRootInit[] value
                    {
                        nRootID = InitID;
                    }
                    else // not the first time we've seen this root node type, so use the next even delta after the next id for this root node type
                    {
                        var nDelta = arRootDelta[sCurNodeName];
                        nRootID = InitID + (Math.floor((nCurMaxRootID - InitID) / nDelta) + 1) * nDelta;
                    }
                }

                if((nCurMaxRootID == undefined) || (nRootID > nCurMaxRootID))
                {
                    arRootMaxID[sRootElementName] = nRootID;    // preserve this for the next root node we automatically assign an ID to
                }

                arRootID[sRootElementName] = nRootID;   // preserve this root ID value as the next ID to be used for a node in the tree below this root
            }
            else if(sRootElementName == "") // ERROR - if this node isn't a root node we better already have the root remembered
            {
                // 8345 = ERROR_DS_ILLEGAL_SUPERIOR - The object cannot be added because the parent is not on the list of possible superiors
                throw(new Error(8345, node.nodeName + " appears before a recognized root element, CommandName can not be generated.  Fix " + sRibbonPath + " or update " + WScript.ScriptName + ".\n" + node.xml));
            }

            if(nCmdID == 0) // we need to generate an Id
            {
                nCmdID = FindFreeCmdID(nodeNewViews);
            }//nCmdID == 0

            // write Id into .new.ribbon file
            var nodeNew = nodeNewViews.selectSingleNode("//*[@CommandName=\"" + sCommandName + "\"]");
            if(nodeNew != null) // nodes added from an <Include Src="foo.ribbon"/> won't be present in the original and so will return null here
            {
                XmlWriteAttr(nodeNew, "Id", nCmdID);
            }

            // generate comment

            // clear entire command name stack if root node, otherwise just below current node depth
            arCmdStack.splice((undefined != InitID) ? 0 : (nDepth+1), arCmdStack.length);

            arCmdStack[nDepth] = sName;

            var sComment = XmlReadAttr(node, "Comment");  // <Application.Views> nodes can specify a Comment attr instead of us generating one
            if(sComment != undefined)
            {
                node.attributes.removeNamedItem("Comment"); // remove it since this is one of our enhanced attributes that UICC/IntentCL doesn't support
            }
            if((sComment == undefined) || (sComment == "")) // comment not provided so generate one based on the hierarchy of this node
            {
                sComment = "";
                for(var n=0 ; n < arCmdStack.length ; n++)
                {
                    if(arCmdStack[n] != undefined)
                    {
                        sComment +=  "-" + arCmdStack[n];
                    }
                }
                sComment = sComment.substr(1);
            }

            if(nodeCommand == null)
            {
                // TODO insert new node at appropriate point sorted by Id values, currently just appends to end of list
                nodeCommand = XmlGetElem(nodeCommandsRoot, "Command", "Name", sCommandName, 4);
                
                if(undefined != InitID) // root node
                {
                    XmlInsertCommentBefore(nodeCommand, "#### " + sName + " ####", "\n\n", "\n");
                }

                // TODO it is possible that an existing <Command> record could exist in the top section of the .ribbon
                // and so we wouldn't be in this case and thus wouldn't add this IDC_ to the rcxml.  But since this isn't
                // how I advocate using RibbonDUI.js, I'm not fixing it for now.
                
                AddRCNode("AddH", null, null, "#define "+sCommandName+"  " + nCmdID + "  // " + node.nodeName + ": " + sComment);
            }

            XmlWriteAttr(nodeCommand, "Symbol", sCommandName);
            XmlWriteAttr(nodeCommand, "Id", nCmdID);
            XmlWriteAttr(nodeCommand, "Comment", sComment);

            //WScript.Echo(node.nodeName + " - " + sCurNodeName + " - " + sCommandName + " - " + nCmdID + " - " + arRootID[sRootElementName] + " - '" + sComment.substr(1) + "'");

            // NOTE LabelDescription & KeyTip occupy same stringtable entry (nCmdID+1) so we're relying on both not being needed
            //   at this time KeyTip is not valid on menu items (like in appmenu) since it uses &amp; in the LabelTitle for keytips
            //   and LabelDescription is only valid on menu items
            AddCommandChild(node, "LabelTitle",         nodeCommand, nCmdID,   sName + "Label",     "LabelTitle: " + sComment);
            AddCommandChild(node, "LabelDescription",   nodeCommand, nCmdID+1, sName + "Desc",      "LabelDescription: " + sComment);
            AddCommandChild(node, "Keytip",             nodeCommand, nCmdID+1, sName + "Keytip",    "{MaxLength=2} Keytip: " + sComment);
            AddCommandChild(node, "TooltipTitle",       nodeCommand, nCmdID+2, sName + "TipTitle",  "TooltipTitle: " + sComment);
            AddCommandChild(node, "TooltipDescription", nodeCommand, nCmdID+3, sName + "TipDesc",   "TooltipDescription: " + sComment);

            var sBitmap = XmlReadAttr(node, "Bitmap");  // specify Bitmap="" to force no bitmap or Bitmap="file.png" to force a specific file
            if(sBitmap != undefined)                             // Bitmap attr is present
            {
                node.attributes.removeNamedItem("Bitmap");  // remove it since this is one of our enhanced attributes that UICC/IntentCL doesn't support
            }
            else if(arBmpNodes[node.nodeName] != undefined) // no Bitmap attr present AND this is a node we generate a bitmap reference for
            {
                sBitmap = "png/Ribbon" + sName + ".png";    // use auto generated pathname
            }
            else
            {
                sBitmap="";
            }

            var sBitmapLoc = XmlReadAttr(node, "BitmapLoc");    // specify BitmapLoc="true" to add bitmap to the localized DLL instead of the non-localized
            if(sBitmapLoc != undefined)                         // Bitmap attr is present
            {
                node.attributes.removeNamedItem("BitmapLoc");   // remove it since this is one of our enhanced attributes that UICC/IntentCL doesn't support
            }

            if(sBitmap != "")
            {
                // support [cmdFoo] indicating the bitmap for the item should come from this command
                var ar = /^\[(.+)\]$/.exec(sBitmap);
                if(ar != null)
                {
                    AddRCNode("AddRC", nCmdID+2, null, (nCmdID+2) + " RCDATA {" + ar[1] + "} // BitmapInfo: " + sCommandName, null, null, false);                
                }
                else
                {
                    var fLoc = TrueFalseNull(sBitmapLoc);
                    AddRCNode("Image", nCmdID, "IDB_Ribbon" + sName, sBitmap, false, "Bitmap(96dpi): " + sComment, fLoc);
                    
                    arSplit = reSplitPath.exec(sBitmap);

                    var sPath = arSplit[1] + arSplit[2] + ".144" + arSplit[3];
                    if(fso.FileExists(sRcxmlDir + sPath))
                    {
                        AddRCNode("Image", nCmdID+1, "IDB_Ribbon" + sName + "144", sPath, false, "Bitmap(144dpi): " + sComment, fLoc);
                    }

                    sPath = arSplit[1] + arSplit[2] + ".HC.bmp";
                    if(fso.FileExists(sRcxmlDir + sPath))
                    {
                        AddRCNode("Bitmap", nCmdID, "IDB_Ribbon" + sName + "HC", sPath, null, "Bitmap(HC): " + sComment, fLoc);
                    }
                }
            }

            // handle our enhanced nodes which UICC/IntentCL won't compile
            if(/^(Item|ItemCategory)$/.test(node.nodeName))
            {
                ProcessItemList(node, nCmdID);
            }
            else if(/^(ExtraCommand)$/.test(node.nodeName))
            {
                XmlRemoveNode(node);
            }
        }//for nl

        ProcessItemList(null, 0);   // flush any list we've still got

        /**************************************************************************
          STEP 4 - final verification
         **************************************************************************/

        // To allow an optimization in the product code we want to ensure that for each
        // list of <Item> elements that there are no command Ids from outside the list
        // with values in the min-max range of the list's <Item> command Id values.

        while(arItemListRanges.length > 0)
        {
            var nMin = arItemListRanges.shift();
            var nMax = arItemListRanges.shift();
            var sMatch = arItemListRanges.shift();
            var sListCmdName = arItemListRanges.shift();
            var reMatch = new RegExp(sMatch);

            WScript.Echo("... checking " + sListCmdName + " range [" + nMin + "," + nMax + "] \n    " + sMatch);

            //<Command Name="cmdAppMenu" Symbol="cmdAppMenu" Id="10000" Comment="AppMenu">
            // find all direct child Command elements with Id attribute values between nMin and nMax
            nl = nodeCommandsRoot.selectNodes("n:Command[@Id >= " + nMin + " and @Id <= " + nMax + "]");
            while(null != (node = nl.nextNode()))
            {
                var sID = XmlReadAttr(node, "Id");
                if(reMatch.test(sID)) continue; // this command is in the list so its fine

                var sName = XmlReadAttr(node, "Name");

                WScript.Echo("Warning: " + sName + " Id=" + sID + " sits in the " + sListCmdName + " list range [" + nMin + "," + nMax + "].  You should re-assign Id values if possible to keep list Item Ids in a compact continguous block.  ItemCategory can be assigned a new Id to make a hole if needed.");
            }
        }

        /**************************************************************************
          STEP 5 - save out generated files
         **************************************************************************/

        // Supports <Ribbon /> elements with "Target" attributes set to generate multiple "post" ribbon files.
        WritePostRibbonResources(xmlRibbon, sObjDir, sPostRibbonName);

        WScript.Echo("\nWriting: " + sSourceDir + sNewRibbonName);
        xmlNew.save(sSourceDir + sNewRibbonName);

        WScript.Echo("\nWriting: " + sRcxmlDir + sRCName);
        xmlRC.save(sRcxmlDir + sRCName);

        return 0;
    }
    catch (e)
    {
        WScript.Echo("ERROR(" + e.number + "): " + e.description);
        xmlRibbon.save(sSourceDir + sErrRibbonName);
        //throw(e); // uncomment to debug - causes cscript to display line number of source of error
        return e.number;
    }

}//PreprocessRibbonMarkup()

function SetNamespace(xmlChunk, sNamespace)
{
    var nl = xmlChunk.selectNodes("//*[@Symbol]"); // find all descendents with a Symbol attribute
    var node;

    while(null != (node = nl.nextNode()))
    {
        // Read the "Symbol" attribute, strip its prefix (if any), prepend the namespace, and set the new "Symbol"
        var sSymbol = XmlReadAttr(node, "Symbol", "");
        XmlWriteAttr(node, "Symbol", sNamespace + ExtractNameFromCommand(sSymbol, true));
    }
}

function BuildMappingFunction(sName, sSearchAttr)
{
    var sCode = "inline UINT " + sName + "(UINT nID)\r\n{\r\n";
    var nl = nodeRibbonRoot.selectNodes("n:Application.Views//*[@" + sSearchAttr + "]"); // find all descendents with a <sSearchAttr> attribute
    var cMappings = 0;

    for (var i=0; i < nl.length; i++)
    {
        var node = nl.item(i);

        var sValue = XmlReadAttr(node, sSearchAttr, "");
        if(sValue != "")
        {
            var sCommandName = XmlReadAttr(node, "CommandName");
            var sCase = "    case " + sCommandName + ":\r\n";

            if(sCode.search(sCase) == -1)
            {
                if("LargeBitmap" == sSearchAttr && "auto" == sValue.toLowerCase())
                {
                    // Convert the command name to an image resource name.
                    sValue = GenerateLargeImageName(sCommandName);

                    // Generate the <ExtraCommand /> element to be added back to the in-memory ribbon XML.
                    // The <ExtraCommand /> will generate the appropriate RCXML and RibbonID definitions.
                    if(strImageMappingNodes == "")
                    {
                        strImageMappingNodes = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n<ExtraCommands>\r\n";
                    }
                    strImageMappingNodes += "<ExtraCommand CommandName=\"" + sValue + "\" Bitmap=\"png/Ribbon" + ExtractNameFromCommand(sValue, false) + ".png\" ";
                    if(TrueFalseNull(XmlReadAttr(node, "RTL", false)))
                    {
                        // The regular image is RTL-enabled, so the auto-generated large image
                        // also needs to be marked for RTL support.
                        strImageMappingNodes += "RTL=\"true\" ";
                    }
                    strImageMappingNodes += "/>\r\n";
                }

                if(++cMappings == 1)
                {
                    // Now that there's at least one mapping, create a "switch" statement.
                    sCode += "    switch (nID)\r\n    {\r\n";
                }

                sCode += sCase;
                sCode += "        nID = " + sValue + ";\r\n";
                sCode += "        break;\r\n";
            }
        }
        node.attributes.removeNamedItem(sSearchAttr);
    }
    if(0 < cMappings)
    {
        // Close the "switch" statement.
        sCode += "    }\r\n";
    }
    sCode += "    return nID;\r\n}\r\n";

    return sCode;
}// BuildMappingFunction

function BuildRTLMapping()
{
    // The IsFlippedForRTL() function will return true for all ribbon commands marked
    // with RTL="true" in the ribbon markup.
    var sCode = "inline bool IsFlippedForRTL(UINT nID)\r\n{\r\n";
    var nl = nodeRibbonRoot.selectNodes("n:Application.Views//*[@RTL]"); // find all descendents with an RTL attribute
    var cMappings = 0;

    for (var i=0; i < nl.length; i++)
    {
        var node = nl.item(i);

        if(TrueFalseNull(XmlReadAttr(node, "RTL", false)))
        {
            var sCommandName = XmlReadAttr(node, "CommandName");
            var sCase = "    case " + sCommandName + ":\r\n";

            if(sCode.search(sCase) == -1)
            {
                if(++cMappings == 1)
                {
                    // Normalize the ID to its base command (to account for high DPI and HC IDs).
                    sCode += "    nID = ((nID - " + nRcxmlRangeMin + ") & ~3) + " + nRcxmlRangeMin + ";\r\n"

                    // Now that there's at least one mapping, create a "switch" statement.
                    sCode += "    switch (nID)\r\n    {\r\n";
                }

                sCode += sCase;
            }
        }
        node.attributes.removeNamedItem("RTL");
    }
    if(0 < cMappings)
    {
        // Return true for all previous cases.
        sCode += "        return true;\r\n";

        // Close the "switch" statement.
        sCode += "    }\r\n";
    }
    sCode += "    return false;\r\n}\r\n";
    
    return sCode;
} // BuildRTLMapping

function GenerateImageMappings()
{
    var sCode;
    var sGroupMapping = BuildMappingFunction("MapGroupsToImages", "GroupImage");
    var sLargeMapping = BuildMappingFunction("MapImagesToLargeImages", "LargeBitmap");

    // If strImageMappingNodes was filled out by BuildMappingFunction(), it needs to be closed.
    if(strImageMappingNodes != "")
    {
        strImageMappingNodes += "</ExtraCommands>";
    }

    sCode = "// This file is generated by RibbonDUI.js.  DO NOT EDIT.\r\n\r\n";
    sCode += "#pragma once\r\n\r\n" + sGroupMapping + "\r\n" + sLargeMapping;

    return sCode;
}// GenerateImageMappings

function WriteResourceTarget(xmlRibbon, sSourceDir, sTarget, sExt)
{
    // Exclude every ribbon except for the ribbon specified by sTarget
    // and write it into its own "post" ribbon resource file.

    try
    {
        var xmlTarget;
        var xmlRoot;
        var nl;
        var nextRibbon;
        var nodeRibbon;

        WScript.Echo("\nPreparing ribbon resource \"" + sTarget + "\"");

        xmlTarget = xmlRibbon.cloneNode(true /* deep cloning */);
        xmlRoot = XmlGetElem(xmlTarget, "Application", "", null, 4);

        nl = xmlRoot.selectNodes("//n:Ribbon");

        nextRibbon = nl.nextNode(); // Prime the loop.  We're deleting nodes.
        while(null != (nodeRibbon = nextRibbon))
        {
            nextRibbon = nl.nextNode();

            if(XmlReadAttr(nodeRibbon, "Target", "") == sTarget)
            {
                // This is the "target" ribbon, so remove just the "Target" attribute.
                // UICC.exe does not understand the "Target" attribute.
                nodeRibbon.attributes.removeNamedItem("Target");
            }
            else
            {
                // This is not the "target" ribbon, so exclude this node from the final resource.
                XmlRemoveNode(nodeRibbon);
            }
        }

        // By this point, only one <Ribbon /> element should remain in the XML.

        WScript.Echo("Writing: " + sSourceDir + sTarget + ".post" + sExt);
        xmlTarget.save(sSourceDir + sTarget + ".post" + sExt);

        return 0;
    }
    catch (e)
    {
        WScript.Echo("ERROR(" + e.number + "): " + e.description);
        return e.number;
    }
}//WriteResourceTarget


function WritePostRibbonResources(xmlRibbon, sSourceDir, sPostRibbonName)
{
    try
    {
        var cRibbons = 0;
        var cTargetsFound = 0;
        var nl = nodeRibbonRoot.selectNodes("//n:Ribbon"); // find all Ribbon elements
        var nodeRibbon;
        var sExt = undefined;

        while(null != (nodeRibbon = nl.nextNode()))
        {
            var sTarget = XmlReadAttr(nodeRibbon, "Target", "");    // eg Target="Compose"
            if(sTarget != "")
            {
                if (undefined == sExt)
                {
                    var reSplitPath = /^(.+\\|)(.+?)(\.[^\\:\.]+|)$/i  // 1=path 2=filename 3=ext
                    var arSplit = reSplitPath.exec(sPostRibbonName);

                    sExt = arSplit[3];
                }

                WriteResourceTarget(xmlRibbon, sSourceDir, sTarget, sExt);
                cTargetsFound++;
            }
            cRibbons++;
        }

        if(cRibbons > cTargetsFound && cRibbons > 1)
        {
            // As the error describes, if any ribbons have targets, then all ribbons must have targets.
            throw(new Error(8346, "Some <Ribbon /> elements are missing \"Target\" attributes.  If any <Ribbon /> elements have targets, then all must have targets.\n" + nodeRibbon.xml));
        }
        else if(0 == cTargetsFound)
        {
            // There is exactly one ribbon, and it did not contain a "Target" attribute.
            // This is the default case for most ribbon markup files.
            WScript.Echo("\nWriting: " + sSourceDir + sPostRibbonName);
            xmlRibbon.save(sSourceDir + sPostRibbonName);
        }

        return 0;
    }
    catch (e)
    {
        WScript.Echo("ERROR(" + e.number + "): " + e.description);
        return e.number;
    }
}//WritePostRibbonResources

/* Support for <Item> and <ItemCategory> enhanced markup elements
   These are used when you want to add pre-authored items to <ComboBox> and <*Gallery> controls

.ribbon markup examples:

    <ComboBox CommandName="cmdVETransitionSpeed" IsEditable="false" Keytip="L" LabelTitle="Speed:" TooltipTitle="Transition speed" TooltipDescription="Choose the speed of the transition." Bitmap="">
        <Item CommandName="cmdTransitionSpeed1" LabelTitle="Very slow" Value="200" Bitmap="">
        <Item CommandName="cmdTransitionSpeed5" LabelTitle="Very fast" Value="50"  Bitmap=""/>
    </ComboBox>

    <InRibbonGallery CommandName="cmdVETransitionGallery" Type="Items" TextPosition="Hide" MinColumnsLarge="3" ItemWidth="64" ItemHeight="48" Keytip="R" LabelTitle="More transitions" TooltipTitle="Transitions" TooltipDescription="Choose a transition to add between the previous item and the selected item.">
        <Item CommandName="cmdTransitionNone" LabelTitle="No transition"/>
        <Item CommandName="cmdTransitionCrossfade" LabelTitle="Crossfade"/>
    </InRibbonGallery>

    <InRibbonGallery CommandName="cmdVEPanAndZoomEffectGallery" Type="Items" TextPosition="Hide" MinColumnsLarge="2" MaxColumns="2" ItemWidth="64" ItemHeight="48" Keytip="N" LabelTitle="Pan and Zoom" TooltipTitle="Pan and Zoom" TooltipDescription="Applies a pan and zoom effect to a photo.">
        <ItemCategory CommandName="cmdPanAndZoomEffectGroupAutomatic" LabelTitle="Automatic">
            <Item CommandName="cmdPanAndZoomEffectAutomatic" Value="0" LabelTitle="Automatic"/>
        </ItemCategory>
        <ItemCategory CommandName="cmdPanAndZoomEffectGroupPan" LabelTitle="Panning Only">
            <Item CommandName="cmdPanAndZoomEffectPanLeftToRightAlongMiddle" Value="100" LabelTitle="Left to right along middle"/>
            <Item CommandName="cmdPanAndZoomEffectPanLeftToRightAlongTop" Value="101" LabelTitle="Left to right along top"/>
        </ItemCategory>
    </InRibbonGallery>

    <DropDownGallery CommandName="cmdMoviePublish" LabelTitle="&amp;Publish" TooltipTitle="Publish">
        <Item CommandName="cmdGetPublishProviders" LabelTitle="&amp;Add a plug-in..." LabelDescription="Download add-ons so you can publish to more places on the web." Bitmap=""/>
    </DropDownGallery>

    <SplitButtonGallery>

.rcxml output will look like this (these match the cmdVEPanAndZoomEffectGallery <ItemCategory> and <Item> elements above):
    <AddRC ID="11324" Value="11324 RCDATA {0,11332,0L,1,11340,100L,1,11348,101L} // ItemList: cmdVEPanAndZoomEffectGallery" Localized="false"/>
    <AddRC ID="11325" Value="11325 RCDATA {11328,11336} // CategoryList: cmdVEPanAndZoomEffectGallery" Localized="false"/>
*/                                        
function ProcessItemList(nodeItem, nCmdID)
{
    var sCategoryID = undefined;
    var nParentID   = 0;
    var sParentCmdName;

    if(nCmdID != 0) // 0 passed at end of processing to force flush arItemListCmds
    {
        // locate the List's parent for this <Item> element and note any <MenuGroup> as nCategoryID
        var reListParent = /^(ComboBox|InRibbonGallery|DropDownGallery|SplitButtonGallery)$/;
        for(var node=nodeItem ; node != null ; node = node.parentNode)
        {
            if(node.nodeName == "ItemCategory")
            {
                if(sCategoryID == undefined)    // we only want the first ItemCategory on our walk up the parents
                {
                    try
                    {
                        // get the node's CommandName attr value, look up the <Command Name=""> element that matches, read its Id attr value
                        sCategoryID = XmlReadAttr(XmlGetElem(nodeCommandsRoot, "Command", "Name", XmlReadAttr(node, "CommandName"), 0), "Id");
                    }
                    catch(e)
                    {
                    }
                }
            }
            else if(reListParent.test(node.nodeName))
            {
                try
                {
                    sParentCmdName = XmlReadAttr(node, "CommandName");
                    nParentID = parseInt(XmlReadAttr(XmlGetElem(nodeCommandsRoot, "Command", "Name", sParentCmdName, 0), "Id"));
                }
                catch(e)
                {
                }

                break; // we found the list parent so don't look any higher
            }
        }
    }

    if(arItemListCmds[1] != nParentID)  // if the current node's parent cmd ID doesn't match the current list's then we need to flush the current list and start a new one
    {
        // flush the existing ItemList and CategoryList
        var sListCmdName = arItemListCmds.shift();  // parent command ID symbol name  eg "cmdMyGallery"
        var nListID = arItemListCmds.shift();       // parent command ID symbol value eg "10412"
        if(nListID > 0)
        {
            var s = "";
            var arList = new Array();
            var arSortedCategoryList = new Array();

            while(arItemListCmds.length > 0)
            {
                var nCategoryID = arItemListCmds.shift();
                var nCommandID = arItemListCmds.shift();
                var nValue = arItemListCmds.shift();

                // , category cmd ID (WORD), item cmd ID (WORD), item Value (DWORD)
                s += "," + nCategoryID + "," + nCommandID + "," + nValue + "L";

                arList.push(nCommandID);
            }

            if(s.length > 0)
            {
                // for the Item list we'll use RCDATA resource ID = parent command ID
                AddRCNode("AddRC", nListID, null, nListID + " RCDATA {" + s.substr(1) + "} // ItemList: " + sListCmdName, null, null, false);
            }

            if(nCategoryCount > 0)
            {
                // arCategoryList uses the CommandID value as a key, the values are the 0 based indexed into the list of categories
                // since sorting by commandID values won't necessarily match markup order we need to build an index sorted list of
                // the CommandIDs in order to guarantee markup order in the RCDATA list
                for(var sCmdID in arCategoryList)
                {
                    arSortedCategoryList[arCategoryList[sCmdID]] = sCmdID;

                    // since we've now processed any <Item> elements that would have been below this <ItemCategory> we can delete it now
                    try
                    {
                        // look up the <Command Id=""> element that matches, read its Name attr value, get the <ItemCategory> node matching the CommandName attr value
                        var nodeCommand = XmlGetElem(nodeCommandsRoot, "Command", "Id", sCmdID, 0);
                        var sName = XmlReadAttr(nodeCommand, "Name");
                        var nodeItemCategory = nodeRibbonRoot.selectSingleNode('n:Application.Views//n:ItemCategory[@CommandName="' + sName + '"]');
                        XmlRemoveNode(nodeItemCategory);
                    }
                    catch(e)
                    {
                    }
                }//for
                
                // for the Category list we'll use RCDATA resource ID = parent command ID + 1
                AddRCNode("AddRC", nListID+1, null, (nListID+1) + " RCDATA {" + arSortedCategoryList.join(",") + "} // CategoryList: " + sListCmdName, null, null, false);
            }

            if(arList.length > 0)
            {
                var nodeListParent = nodeRibbonRoot.selectSingleNode('n:Application.Views//*[@CommandName="' + sListCmdName + '"]');
                var sType = XmlReadAttr(nodeListParent, "Type", "Commands"); // TODO docs don't say what the default type is, being conservative since we track commands only
                if(sType == "Commands")
                {
                    // Keep track of the min and max command ID for each ItemList for verification step
                    var arSorted = arList.sort(NumericSortCompare)
                    var nMin = arSorted[0]; // note we're not including the category Ids in the min-max because they don't impact the product code optimization
                    var nMax = arSorted[arSorted.length - 1];
                    var sMatch = "^(" + arSorted.join("|");
                    if(arSortedCategoryList.length > 0) sMatch += "|" + arSortedCategoryList.join("|");
                    sMatch += ")$";
                    arItemListRanges.push(nMin, nMax, sMatch, sListCmdName);
                }
            }
        }

        // begin the new ItemList and CategoryList
        arItemListCmds.length = 0;      // remove all items in array
        arItemListCmds.push(sParentCmdName, nParentID);  // store the list's parent as the first pair of items in the array

        nCategoryCount = 0;
        arCategoryList.length = 0;
    }//flush

    if(nCmdID != 0)  // not just force flushing at end, so add the <Item> to list
    {
        var nCategoryID = -1;    // no category for this item by default
        if(sCategoryID != undefined)
        {
            if(arCategoryList[sCategoryID] == undefined)    // if command ID is not already in the category list, add it
            {
                arCategoryList[sCategoryID] = nCategoryCount++;
            }
            nCategoryID = parseInt(sCategoryID, 10);

            // we can't remove the <ItemCategory> here because it would orphan (separate from the parent)
            // any child <Item> elements which we haven't yet processed
        }

        if(sCategoryID != nCmdID)   // The <Item>'s nCmdID won't equal the <ItemCategory>'s sCategoryID, so we know this is an <Item>, so add it to the list
        {
            var nValue = parseInt(XmlReadAttr(nodeItem, "Value", "0"));
            arItemListCmds.push(nCategoryID, nCmdID, nValue);

            // remove our enhanced <Item> element from the .post.ribbon so UICC/IntentCL doesn't see it
            XmlRemoveNode(nodeItem);
        }
    }
}//function ProcessItemList

/* from this under <Application.Views>:
        <Button CommandName="cmdAssetAdd"    KeyTip="A" LabelTitle="Add"    ToolTipTitle="Add videos and photos" ToolTipDescription="Add videos and photos to the project."/>

   generate this under <Application.Commands>:
        <Command Name="cmdAssetAdd" Symbol="cmdAssetAdd" Id="10408" Comment="HomeTab-HomeAssetGroup-AssetAdd">
            <Command.LabelTitle>        <String Id="10408" Symbol="strAssetAddLabel"   >Add</StringDef></Command.LabelTitle>
            <Command.KeyTip>            <String Id="10409" Symbol="strAssetAddKeyTip"  >A</StringDef></Command.KeyTip>
            <Command.ToolTipTitle>      <String Id="10410" Symbol="strAssetAddTipTitle">Add videos and photos</StringDef></Command.ToolTipTitle>
            <Command.ToolTipDescription><String Id="10411" Symbol="strAssetAddTipDesc" >Add videos and photos to the project.</StringDef></Command.ToolTipDescription>
        </Command>

   AddCommandChild(node, "LabelTitle", nodeCommand, nCmdID, sName + "Label", sComment);
*/
function AddCommandChild(node, sAttrName, nodeCommand, nID, sName, sComment)
{
    var sAttrVal = XmlReadAttr(node, sAttrName);
    if(sAttrVal != undefined)
    {
        node.attributes.removeNamedItem(sAttrName);

        var nodeCommandDot = XmlGetElem(nodeCommand, "Command." + sAttrName, "", null, 4);

        var nodeChild = XmlSetNodeText(nodeCommandDot, "String", sAttrVal, "");
        XmlWriteAttr(nodeChild, "Id", nID);
        XmlWriteAttr(nodeChild, "Symbol", "str" + sName);

        AddRCNode("String", nID, "IDS_Ribbon" + sName, sAttrVal, null, sComment);
    }
}

/* RCXML lines we'll add            
    <Image  ID="11001" Name="IDB_RibbonHomeAssetAdd" Value="png/RibbonHomeAssetAdd.png" RLE="false" />
    <String ID="11001" Name="IDS_RibbonHomeAssetAdd" Value="Add"/>
*/
function AddRCNode(sTag, nID, sName, sValue, fRLE, sComment, fLocalized)
{
    var node = null;
    if(nID != null)
    {
        node = nodeRCRoot.selectSingleNode(sTag + "[@ID=\"" + nID + "\"]");

        // RCXML defaults <Bitmap> elements to localized which is wrong for our usage for High Contrast bitmaps
        // so if this <Bitmap ID> doesn't exist already (2nd use of same CommandName) and BitmapLoc attr wasn't specified we'll force it Localized="false"
        if((node == null) && (fLocalized == null) && (sTag == "Bitmap"))
        {
            fLocalized = false;
        }
    }
    if(node == null)
    {
        var doc = nodeRCRoot.ownerDocument;
        node = doc.createNode(1, sTag, nodeRCRoot.namespaceURI);
        nodeRCRoot.appendChild(node);
        nodeRCRoot.appendChild(doc.createTextNode("\n"));

        if(nID != null) XmlWriteAttr(node, "ID", nID);
    }

    if(sName != null) XmlWriteAttr(node, "Name", sName);

    XmlWriteAttr(node, "Value", sValue);

    if(fRLE != null) XmlWriteAttr(node, "RLE", fRLE ? "true" : "false");

    if(sComment != null) XmlWriteAttr(node, "LocComment", sComment);

    if(fLocalized != null) XmlWriteAttr(node, "Localized", fLocalized ? "true" : "false");
}

// Returns a attribute override dictionary (attribute name, attribute value) for the passed in element
function GetAttributeOverrides(node)
{
    var attributeOverrides = new ActiveXObject("Scripting.Dictionary");

    for (var attributeIndex in arAttributeOverrides)
    {
        var sAttributeValue = XmlReadAttr(node, arAttributeOverrides[attributeIndex], "");
        if (sAttributeValue != "")
           attributeOverrides.add(arAttributeOverrides[attributeIndex], sAttributeValue);
    }

    return attributeOverrides;
}

// Uses the attribute override dictionary to update the attributes on the passed in element subtree                        
function ApplyAttributeOverrides(node, attributeOverrides)
{
    var a = new VBArray(attributeOverrides.Keys()).toArray();
    for (var attribute in a)
    {
        var sAttributeName = arAttributeOverrides[attribute];
        var nl = node.selectNodes("//n:*[@" + sAttributeName +"]"); // find all elements with this attribute

        var nodeToOverride;
        while(null != (nodeToOverride = nl.nextNode()))
        {
            var attributeValue = attributeOverrides.Item(sAttributeName);
            nodeToOverride.setAttribute(sAttributeName, attributeValue);
        }
    }
}

// sAttrName="" for no sAttrName attr query filter
// assumes schema prefix is n: for all nodes
function XmlGetElem(nodeParent, sNodeName, sAttrName, sAttrVal,
            fCreate/*0=don't create 1=create 2=create + sInsert after 3=create + sInsert before 4=create + smart indent*/,
            sInsert, nodeInsertBefore)
{
    var sQuery = "n:" + ((sAttrName == "") ? sNodeName : sNodeName + "[@" + sAttrName + "=\"" + sAttrVal + "\"]");

    var node = nodeParent.selectSingleNode(sQuery);

    if( (node == null) && (fCreate != 0) )
    {
        var doc = nodeParent.ownerDocument;
        node = doc.createNode(1, sNodeName, nodeParent.namespaceURI);

        if(sAttrName != "")
        {
            var attr = doc.createAttribute(sAttrName);
            attr.value = sAttrVal;
            node.attributes.setNamedItem(attr);
        }

        XmlInsertNode(nodeParent, node, fCreate, sInsert, nodeInsertBefore);
    }

    return node;
}

// used by XmlGetElem() when creating new nodes
function XmlInsertNode(nodeParent, node,
            fCreate/*1=insert 2=insert + sInsert after 3=insert + sInsert before 4=insert + smart indent*/,
            sInsert, nodeInsertBefore)
{
    if((nodeInsertBefore == undefined) || (nodeInsertBefore == null))
    {
        // caller didn't provide nodeInsertBefore so we'll append node after any existing children of nodeParent
        // if nodeParent.lastChild is a text node, it is "indenting" the closing tag of nodeParent so we insert before that

        nodeInsertBefore = nodeParent.lastChild;   // assume we'll insert before lastChild, note this will be null if there are no children
        if((nodeInsertBefore != null) && (nodeInsertBefore.nodeType != XmlNodeType_Text))  // if lastChild isn't text, insert after that last child
        {
            nodeInsertBefore = null; //null = append after existing children
        }
    }
    else if((nodeInsertBefore.previousSibling != null) && (nodeInsertBefore.previousSibling.nodeType == XmlNodeType_Text))
    {
        // if there is a text node (indent) before the node caller wants us to insert before, insert before the text node
        nodeInsertBefore = nodeInsertBefore.previousSibling;
    }

    var nodeInserted = nodeParent.insertBefore(node, nodeInsertBefore);

    if(fCreate == 2)        // sInsert after new node
    {
        nodeParent.insertBefore(nodeParent.ownerDocument.createTextNode(sInsert), nodeInsertBefore);
    }
    else if(fCreate == 3)   // sInsert before new node
    {
        nodeParent.insertBefore(nodeParent.ownerDocument.createTextNode(sInsert), nodeInserted);
    }
    else if(fCreate == 4)
    {
        // use whitespace before our parent, its siblings or equivalent of a grandparent to determine indent amount for our new node
        sInsert = XmlFindParentWhitespace(nodeParent, "\n") + ksIndent;
        //var nodeText = nodeParent.insertBefore(nodeParent.ownerDocument.createTextNode(sInsert), node);
        XmlIndentTree(nodeInserted, sInsert);
    }

    return nodeInserted;
}

function XmlIndentTree(node, sIndent)   // calls itself recursively
{
    // "indent" our opening tag
    node.parentNode.insertBefore(node.ownerDocument.createTextNode(sIndent), node);

    var nl = node.childNodes;
    var nodeChild;
    while(null != (nodeChild = nl.nextNode()))
    {
        if(nodeChild.nodeType == XmlNodeType_Text)
        {
            node.removeChild(nodeChild);    // remove any existing child text nodes (existing indenting)
        }
        else
        {
            XmlIndentTree(nodeChild, sIndent + ksIndent);   // recursively process non-text child nodes
        }
    }

    if(node.lastChild != null)  // if we have children then we'll have a closing tag so "indent" it
    {
        node.appendChild(node.ownerDocument.createTextNode(sIndent));
    }
}

// locate the text node before our parent, its previous siblings or equivalent of a grandparent
function XmlFindParentWhitespace(nodeParent, sDefault)
{
    var node = nodeParent;
    while(node != null)
    {
        while( node.previousSibling != null )
        {
            node = node.previousSibling;
            
            if(node.nodeType == XmlNodeType_Text)
            {
                var re = /(\n[\t ]*)$/; // the last \n in the string and any tab or space characters that follow it
                var arResult = re.exec(node.text);
                if(arResult != null) return arResult[1];
                return sDefault;
            }
        }

        node = node.parentNode;
    }
    return sDefault;
}

function XmlRemoveNode(node)    // removes node and any adjacent sibling text nodes
{
    var nodeParent = node.parentNode;

    // remove any adjacent later siblings that are text nodes (type=3) or comment nodes (type=8)
    var n=node.nextSibling;
    while( (n != null) && ((n.nodeType == XmlNodeType_Text) || (n.nodeType == XmlNodeType_Comment)) )
    {
        var next = n.nextSibling;
        nodeParent.removeChild(n);
        n = next;
    }

    if(n == null)   // we were the last non-text node in the list of children
    {
        // see if there are any non-text sibling nodes before us
        for(n=node.previousSibling ; (n != null) && ((n.nodeType == XmlNodeType_Text) || (n.nodeType == XmlNodeType_Comment)) ; n = n.previousSibling);
    }

    if(n == null)   // if n is still null that means we got all the way to begining of the children without finding a non-text node
    {
        // so remove all the children of our parent (ie any text + us)
        while(nodeParent.firstChild != null) nodeParent.removeChild(nodeParent.firstChild);
    }
    else    // otherwise, just remove us
    {
        nodeParent.removeChild(node);
    }
}

function XmlRemoveNodes(nlRemoveNodes, keepChildren)
{
    var nodeToRemove;

    while(null != (nodeToRemove = nlRemoveNodes.nextNode()))
    {
        if(keepChildren)
        {
            var nlNodeToRemoveChildren = nodeToRemove.childNodes;
            var nodeToRemoveChild;

            while(null != (nodeToRemoveChild = nlNodeToRemoveChildren.nextNode()))
            {
                XmlInsertNode(nodeToRemove.parentNode, nodeToRemoveChild, 4, null, nodeToRemove);
            }
        }

        XmlRemoveNode(nodeToRemove);
    }
}

function XmlWriteAttr(nodeParent, sAttrName, sAttrVal)
{
    var attr = nodeParent.ownerDocument.createAttribute(sAttrName);
    attr.value = sAttrVal;
    nodeParent.attributes.setNamedItem(attr);
}

function XmlReadAttr(nodeParent, sAttrName, sDefaultVal)    // sDefaultVal is returned when the attr doesn't exist, undefined if sDefaultVal is not passed
{
    var a = nodeParent.attributes.getNamedItem(sAttrName);
    return (a == null) ? sDefaultVal : a.value;
}

// assumes schema prefix is n: for all nodes
function XmlGetNodeText(nodeParent, sNodeName)
{
    var node = nodeParent.selectSingleNode("n:" + sNodeName);
    return (node != null) ? node.text : "";
}

// assumes schema prefix is n: for all nodes
function XmlSetNodeText(nodeParent, sNodeName, sTxt, sBefore)
{
    var node = nodeParent.selectSingleNode("n:" + sNodeName);
    if(node == null)
    {
        var doc = nodeParent.ownerDocument;
        node = doc.createNode(1, sNodeName, nodeParent.namespaceURI);
        nodeParent.appendChild(node);
        if(sBefore != "") nodeParent.insertBefore(doc.createTextNode(sBefore), node);
    }
    node.text = sTxt;
    return node;
}

function XmlInsertCommentBefore(node, sTxt, sLFBefore, sLFAfter)
{
    var nodeInserted = XmlInsertNode(node.parentNode, node.ownerDocument.createComment(sTxt), 4, null, node);

    if(sLFBefore != "") nodeInserted.previousSibling.text = sLFBefore + nodeInserted.previousSibling.text;
    if(sLFAfter != "") nodeInserted.nextSibling.text = sLFAfter + nodeInserted.nextSibling.text;
}

function XmlSetSelectionNamespace(node)
{
    // defines n: namespace prefix for the .ribbon file's required namespace so we can refer to n:nodename in our queries
    node.setProperty("SelectionNamespaces", "xmlns:n=\"" + sNamespaceURI + "\"");
}

function TrueFalseNull(sAttrVal)
{
    if((sAttrVal == undefined) || (sAttrVal == null)) return null;
    return /true|yes|1/i.test(sAttrVal);
}

function EnsureTrailingBackslash(sPath)
{
    return (/(^|\\)$/.test(sPath)) // is path empty OR ending in a \
        ? sPath : (sPath + "\\");
}

function NumericSortCompare(a,b)
{
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}
