<?php

namespace FBS\Model;


class PatronSettingsV3 {

  /**
   * @var string Required if patron should receive email notifications
   */
  public $emailAddress = null;

  /**
   * @var string Language in which the patron prefers the communication with
   *   the library to take place If left empty default library language will
   *   be used
   */
  public $preferredLanguage = null;

  /**
   * @var string Required if patron should receive SMS notifications
   */
  public $phoneNumber = null;

  /**
   * @var string ISIL-number of preferred pickup branch
   * @required
   */
  public $preferredPickupBranch = null;

  /**
   * @var Period If not set then the patron is not on hold
   */
  public $onHold = null;

  /**
   * @var boolean
   * @required
   */
  public $receiveEmail = null;

  /**
   * @var boolean
   * @required
   */
  public $receivePostalMail = null;

  /**
   * @var boolean
   * @required
   */
  public $receiveSms = null;
}
