#!/bin/bash

echo "request=smtpd_access_policy
protocol_state=RCPT
protocol_name=SMTP
helo_name=some.domain.tld
queue_id=8045F2AB23
sender=foo@bar.tld
recipient=bar@foo.tld
recipient_count=0
client_address=1.2.3.4
client_name=www.domain.tld
reverse_client_name=www.domain.tld
instance=123.456.7" | nc localhost 1234
