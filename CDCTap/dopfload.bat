if %1a==a goto err
set tape=upmrr0.tap
Debug\CDCTap -a pfload -m %1 %tape%
goto end
:err
echo Usage: pfload wildcard
:end