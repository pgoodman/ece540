void match(int c)
{
  int tuna,salmon,albatross,woodpecker,three_toed_sloth,pteradactyl,
	duck_billed_platapus;

  switch (c) {
    case 0:	tuna = 1; break;
    case 1:	salmon = 1; break;
    case 2:	albatross = 1; break;
    case 3:	woodpecker = 1; break;
    case 4:	three_toed_sloth = 1; break;
    case 5:	pteradactyl = 1; break;
    default:	duck_billed_platapus = 1; break;
  }

  printf ("lunch ");
  if (tuna+salmon > 0)
    printf ("is fishy");
  if (albatross+woodpecker > 0)
    printf ("is feathery");
  if (three_toed_sloth+duck_billed_platapus > 0)
    printf ("is yucky(!?!)");
  if (pteradactyl > 0)
    printf ("tastes like chicken");
  printf ("\n");
}
