<?php
/**
 * Created by PhpStorm.
 * User: AD
 * Date: 05.05.2017
 * Time: 11:34
 */

namespace FBS\Model;


class CreatePatronRequestV3 {

  /**
   * @var string
   * @required
   */
  public $cprNumber = null;

  /**
   * @var string
   * @required
   */
  public $pincode = null;

  /**
   * @var PatronSettingsV3
   * @required
   */
  public $patron = null;
}
