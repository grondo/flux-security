ARG IMAGESRC

FROM $IMAGESRC

# Allow username, UID, and GID to be overidden on
#  docker build command line:
#
ARG USER=fluxuser
ARG UID=1000
ARG GID=1000
ARG BASE_IMAGE

# Add configured user to image with sudo access:
#
RUN set -x && groupadd -g $UID $USER \
 && useradd -g $USER -u $UID -d /home/$USER -m $USER \
 && printf "$USER ALL= NOPASSWD: ALL\\n" >> /etc/sudoers

# Also add "flux" user to image with sudo access:
#
RUN set -x && groupadd fluxuser \
 && useradd -g fluxuser -d /home/fluxuser -m fluxuser -s /bin/bash \
 && printf "fluxuser ALL= NOPASSWD: ALL\\n" >> /etc/sudoers

# Make sure user in appropriate group for sudo on different platforms
RUN case $BASE_IMAGE in \
     bionic*) adduser $USER sudo && adduser fluxuser sudo ;; \
     focal*)  adduser $USER sudo && adduser fluxuser sudo ;; \
     centos*|fedora*) usermod -G wheel $USER && usermod -G wheel fluxuser ;; \
     *) (>&2 echo "Unknown BASE_IMAGE") ;; \
    esac

# Install extra dependencies if necessary here.
#
# Do not forget to run `apt update` on Ubuntu/bionic.
# Do NOT run `yum upgrade` on CentOS (this will unnecessarily upgrade
#  existing packages)
#
RUN case $BASE_IMAGE in \
     bionic*) ;; \
     focal*) ;; \
     centos*) ;; \
     fedora*) dnf install -y libubsan ;; \
     *) (>&2 echo "Unknown BASE_IMAGE") ;; \
    esac

# Setup MUNGE directories & key
RUN mkdir -p /var/run/munge \
 && dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key \
 && chown -R munge /etc/munge/munge.key /var/run/munge \
 && chmod 600 /etc/munge/munge.key

COPY entrypoint.sh /usr/local/sbin/
COPY bashrc /tmp
RUN cat /tmp/bashrc >> ~fluxuser/.bashrc \
 && rm /tmp/bashrc

ENV BASE_IMAGE=$BASE_IMAGE
USER $USER
WORKDIR /home/$USER
