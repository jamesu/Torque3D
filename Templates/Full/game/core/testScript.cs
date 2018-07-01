
function applyPerkToDB(%perk, %db)  
{  
   eval(%perk);  
}

applyPerkToDB("echo(\"TEST DB = \" @ %db @ \".\");", "Two");
