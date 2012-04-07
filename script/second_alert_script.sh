#!/bin/sh
#
# second alert sample 
#

CALL_DIR=/etc/asterisk/CALL
SPOOL_DIR=/var/spool/asterisk/outgoing
CALL_NUMBER="XXX-XXXX-XXXX"
MAX_RETRY=900

twit="twit"
consumerkey="consumerkey"
consumersecret="consumersecret"
requesttokenuri="https://twitter.com/oauth/request_token"
accesstokenuri="https://twitter.com/oauth/access_token"
authorizeuri="https://twitter.com/oauth/authorize"
tokensavefile="tokensavefile"

if [ "$1" == "gettoken" ]; then
${twit} --gettoken --authtype oauth --consumerkey ${consumerkey} --consumersecret ${consumersecret} --requesttokenuri ${requesttokenuri} --accesstokenuri ${accesstokenuri} --authorizeuri ${authorizeuri} --tokensavefile ${tokensavefile}
   exit 0
fi

/bin/echo "[#IDS ALERT] 侵入者発見したかも。 " | nkf -w | ${twit} --method update --pipe --authtype oauth --tokensavefile ${tokensavefile}

/usr/bin/amixer set Front on 100%
/usr/bin/aplay alert.wav &

if [ ! -f "${CALL_DIR}/call.txt" ];
then
    exit 1
fi

retry=${MAX_RETRY}
while [ -f ${SPOOL_DIR}/call.txt ] && [ ${retry} -gt 0 ] 
do
    /bin/sleep 1
    retry=`/usr/bin/expr ${retry} - 1`
done

/bin/cat ${CALL_DIR}/call.txt | /bin/sed "s/#__CALL_NUMBER__#/${CALL_NUMBER}/g" > ${SPOOL_DIR}/call.txt
