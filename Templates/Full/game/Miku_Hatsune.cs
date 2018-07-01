
echo("Creating MikuDAE");

singleton TSShapeConstructor(MikuDAE)
{
   baseShape = "./art/Miku_Hatsune.pmd";
   loadLights = "0";
   unit = "1.0";
   upAxis = "DEFAULT";
   lodType = "TrailingNumber";
   matNamePrefix = "";
   ignoreNodeScale = "0";
   adjustCenter = "0";
   adjustFloor = "0";
   forceUpdateMaterials = "0";
};

function MikuDAE::onLoad(%this)
{
   %this.addSequence( "./kishimen.vmd ambient", "kishimen", 0, -1);
}
